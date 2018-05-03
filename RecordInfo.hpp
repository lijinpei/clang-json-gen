#pragma once

#include "Directive.hpp"

#include "clang/AST/DeclCXX.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
class FieldDecl;
class CXXRecordDecl;
class CXXBaseSpecifier;
} // namespace clang

// the information needed by codegen functions
struct CodegenContext {
  std::string indent;
  std::string self;             // the source code used to access yourself
  std::string start_state;      // the source code for the start state
  std::string expact_key_state; // the source code for the expect-key state
  std::string prefix; // the prefix that should be append to your state's name
};

struct Field {
  const clang::FieldDecl *field;
  FieldDirective directive;
  Field(const clang::FieldDecl *field, FieldDirective directive)
      : field(field), directive(directive) {}
};

// this class doesn't record whether this is a virtual base
struct SubClass {
  const clang::CXXBaseSpecifier *base;
  bool omit : 1;
  SubClass(const clang::CXXBaseSpecifier *base) : base(base) {}
};

class RecordInfo {
  // bases means non virtual bases
  std::vector<SubClass> bases;
  // vbases means virtual bases
  std::vector<SubClass> vbases;
  std::vector<Field> fields;
  RecordDirective record_directive;
  clang::CXXRecordDecl *type;
  struct VisitContext {
    std::string state_name;
    std::string self;
    std::string parent;
  };
  // class CB {
  // public:
  // // handle the specified field, no matter it is a
  // // base-field/vbase-field/field
  //   bool handleField(const VisitContext&, const Field&);
  // };
  template <bool VisitBase = true, bool VisitVBase = true,
            bool VisitField = true, typename CB>
  bool Visit(const CodegenContext &, CB &cb);

  // codegen functions
  // the following codegen functions only generate code for non-virtual bases
  bool generateEnumBody(llvm::raw_ostream &, const CodegenContext &);
  bool generateNullBody(llvm::raw_ostream &, const CodegenContext &);
  bool generateBoolBody(llvm::raw_ostream &, const CodegenContext &);
  bool generateIntBody(llvm::raw_ostream &, const CodegenContext &);
  bool generateUintBody(llvm::raw_ostream &, const CodegenContext &);
  bool generateInt64Body(llvm::raw_ostream &, const CodegenContext &);
  bool generateUint64Body(llvm::raw_ostream &, const CodegenContext &);
  bool generateDoubleBody(llvm::raw_ostream &, const CodegenContext &);
  bool generateStringBody(llvm::raw_ostream &, const CodegenContext &);
  bool generateKeyBody(llvm::raw_ostream &, const CodegenContext &);
  bool generateStartArrayBody(llvm::raw_ostream &, const CodegenContext &);
  bool generateEndArrayBody(llvm::raw_ostream &, const CodegenContext &);
  bool generateRawNumberBody(llvm::raw_ostream &, const CodegenContext &);
  void emitFieldCheck(llvm::raw_ostream &os, const CodegenContext &cc,
                      const VisitContext &vc, const Field &f) {
    if (!f.directive.is_required) {
      return;
    }
    std::string check_name = vc.state_name + "_check";
    os << cc.indent << "if (" << check_name << ") {\n";
    os << cc.indent << "  return false\n";
    os << cc.indent << "} else {\n";
    os << cc.indent << "  " << check_name << " = false;\n";
    os << cc.indent << "}\n";
  }

public:
  void setDirective(RecordDirective rd) { record_directive = rd; }

  bool addMember(const Field &f) {
    fields.push_back(f);
    return true;
  }
  bool addMember(Field &&f) {
    fields.push_back(f);
    return true;
  }
  bool emplaceMember(const clang::FieldDecl *field, FieldDirective fd) {
    fields.emplace_back(field, fd);
    return true;
  }

  bool addBase(const SubClass &b) {
    bases.push_back(b);
    return true;
  }
  bool addBase(SubClass &&b) {
    bases.push_back(b);
    return true;
  }
  bool emplaceBase(const clang::CXXBaseSpecifier *base) {
    bases.emplace_back(base);
    return true;
  }

  bool addVBase(const SubClass &b) {
    vbases.push_back(b);
    return true;
  }
  bool addVBase(SubClass &&b) {
    vbases.push_back(b);
    return true;
  }
  bool emplaceVBase(const clang::CXXBaseSpecifier *vbase) {
    vbases.emplace_back(vbase);
    return true;
  }

  void emitCode(llvm::raw_ostream &);
};

RecordInfo *getRecordInfoFromDecl(const clang::CXXRecordDecl *);
