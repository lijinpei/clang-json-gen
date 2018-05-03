#include "JsonGen.hpp"
#include "JsonGenTypeVisitor.hpp"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Comment.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/iterator_range.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <forward_list>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/*
 * This file implements a plugin for clang, which handles #pragma json-gen.
 * It take an input file, and rewrite a seperate output file.
 */

namespace {

class JsonGeneratorConsumer;
class JsonGeneratorAction;

const char *JSONGEN = "jsongen";
const std::string JSONGEN_str = JSONGEN;

const char *startWith(const char *str, const char *substr) {
  while (char c = *(substr++)) {
    if (*(str++) != c) {
      return nullptr;
    }
  }
  return str;
}

struct Config {
  std::string output_file_name;
  std::string log_file_name;
};

#pragma GCC diagnostic push
#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wpedantic"
#elif defined(__clang__)
#pragma GCC diagnostic ignored "-Wc99-extensions"
#endif
const Config default_config = {.output_file_name = JSONGEN_str + ".hpp",
                               .log_file_name = "/tmp/" + JSONGEN_str + ".log"};
#pragma GCC diagnostic pop


class JsonGeneratorAction : public clang::PluginASTAction {
  Config config = default_config;

public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef) override;

  clang::PluginASTAction::ActionType getActionType() override {
    // this action replace the default codegen action, don't try to use this
    // plugin while compile normal codes.
    // use -c/-fsyntax-only to supress compiler errors about missing object file
    return clang::PluginASTAction::ReplaceAction;
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &Args) override {
    bool ret = true;
    int n = Args.size();
#ifndef NDEBUG
    console_logger = spdlog::stderr_color_st("console");
    console_logger->set_level(spdlog::level::trace);
    SPDLOG_INFO(console_logger, "args number: {}", n);
    for (int i = 0; i < n; ++i) {
      SPDLOG_INFO(console_logger, "arg {}: {}", i, Args[i]);
    }
#endif
    using hdl_func = std::function<bool(const char *)>;
    const char *log_str = "log=";
    auto log_hdl = [&](const char *pos) -> bool {
      if (!config.log_file_name.size()) {
        config.log_file_name = pos;
        return true;
      } else {
        SPDLOG_ERROR(console_logger, "error: multiple log file specified");
        return false;
      }
    };
    std::pair<const char *, hdl_func> arg_handlers[] = {{log_str, log_hdl}};
    for (int i = 0; i < n; ++i) {
      bool handled = false;
      bool has_error = false;
      for (auto &hd : arg_handlers) {
        if (const char *pos = startWith(Args[i].c_str(), hd.first)) {
          handled = true;
          if (!hd.second(pos)) {
            has_error = true;
          }
          break;
        }
      }
      if (!handled) {
        SPDLOG_ERROR(console_logger, "arg {}: {} not handled", i, Args[i]);
        ret = false;
        break;
      } else if (has_error) {
        SPDLOG_ERROR(console_logger, "arg {}: {} not legal", i, Args[i]);
        ret = false;
        break;
      };
    }
    if (!ret) {
      SPDLOG_INFO(console_logger, "return error");
      return ret;
    }
#ifndef NDEBUG
    SPDLOG_INFO(console_logger, "log file: {}", config.log_file_name);
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_st>(
        config.log_file_name.c_str(), 1024 * 1024 * 5, 3);
    debug_logger = spdlog::stderr_color_st("debug");
    debug_logger->set_level(spdlog::level::trace);
#endif
    return true;
  }
};



/* This class do the rewrite of the ast to a seperate file */
class JsonGeneratorConsumer : public clang::ASTConsumer {
  JsonGeneratorAction *action;
  clang::ASTContext *ast_context = nullptr;
  bool has_error = false;
  template <typename T, typename Alloc = std::allocator<T>>
  class single_list : std::forward_list<T, Alloc> {
    using BaseTy = std::forward_list<T, Alloc>;
    using typename BaseTy::iterator;
    using typename BaseTy::value_type;
    iterator back_;

  public:
    void push_back(const value_type &val) {
      if (back_ != this->end()) {
        insert_after(back_, val);
      } else {
        back_ = push_front(val);
      }
    }
    void push_back(value_type &&val) {
      if (back_ != this->end()) {
        insert_after(back_, val);
      } else {
        back_ = push_front(val);
      }
    }
    template <class... Args> void emplace_back(Args &&... args) {
      if (back_ != this->end()) {
        back_ = empalce_after(back_, std::forward<Args>(args)...);
      } else {
        back_ = emplace_front(std::forward<Args>(args)...);
      }
    }
    iterator back() { return back_; }
  };

  struct TypeWithDirective {
    clang::Type *type;
    std::forward_list<TypeWithDirective *> child_fields;
  };

  // This is the list of type decls we need to generate json parser for, and
  // they are placed in order so that their dependences are met
  single_list<TypeWithDirective> decls_to_emit;
  // map a decl to corresponding item in decls_to_emit
  llvm::DenseMap<clang::Type *, TypeWithDirective *> visited_decl;

  JsonGenTypeVisitor visitor;
  bool addType(const clang::Type *type) { return visitor.Visit(type); }

  bool addDecl(clang::CXXRecordDecl *decl,
                   const clang::comments::FullComment * fc) {
    return visitor.addDecl(decl, fc);
  }

public:
  JsonGeneratorConsumer(JsonGeneratorAction *action) : action(action) {}

  void Initialize(clang::ASTContext &C) override { ast_context = &C; }

  void HandleTagDeclDefinition(clang::TagDecl *D) {
    SPDLOG_INFO(debug_logger, "HandleTagDeclDefinition({})",
                D->getName().str());
    if (has_error) {
      SPDLOG_INFO(debug_logger,
                  "HandleTagDeclDefinition() return: previous error");
      return;
    }
    clang::CXXRecordDecl *decl = llvm::dyn_cast<clang::CXXRecordDecl>(D);
    if (!decl) {
      SPDLOG_INFO(debug_logger,
                  "HandleTagDeclDefinition() return: not a CXXRecordDecl");
      return;
    }
    if (decl->isLocalClass()) {
      SPDLOG_INFO(debug_logger,
                  "HandleTagDeclDefinition() return: isLocalClass");
      return;
    }
    const clang::comments::FullComment *fc =
        ast_context->getCommentForDecl(decl, nullptr);
    if (!fc) {
      SPDLOG_INFO(debug_logger,
                  "HandleTagDeclDefinition() return: no associated comment");
      return;
    }
    if (!addDecl(decl, fc)) {
      has_error = true;
      SPDLOG_INFO(
          debug_logger,
          "HandleTagDeclDefinition() return: error while parse comment");
      return;
    }
    return;
  }
};

std::unique_ptr<clang::ASTConsumer>
JsonGeneratorAction::CreateASTConsumer(clang::CompilerInstance &,
                                       llvm::StringRef) {
  return llvm::make_unique<JsonGeneratorConsumer>(this);
}

static clang::FrontendPluginRegistry::Add<JsonGeneratorAction>
    X(JSONGEN, "generate json reader/writer for your struct");
} // anonymous namespace

