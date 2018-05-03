#include "RecordInfo.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

namespace {
const char *return_true = "return true\n";
const char *return_false = "return false\n";

std::string substituteDoubleDollar(const std::string & str, const std::string & substr) {
  std::string ret;
  size_t lp = 0;
  for (size_t i = 0, s = str.size() - 1; i < s; ++i) {
    if (str[i] == '$') {
      if (str[i + 1] == '$') {
        ret += str.substr(lp, i - lp) + substr;
        lp = i + 2;
      }
      ++i;
    }
  }
  ret += str.substr(lp, str.size() - lp);
  return str;
}
/*

void generateNull(llvm::raw_ostream &os, clang::QualType qt,
                  const std::string &indent, const std::string &self) {
  os << indent;
  const clang::Type *type = qt.getTypePtr();
  if (type->isBooleanType()) {
    os << self << " = false\n";
  } else if (type->isIntegerType()) {
    os << self << " = 0;\n";
  } else if (type->isPointerType()) {
    os << self << "= nullptr;\n";
  } else {
    // I think this will default construct a object
    os << self << " = {};\n";
  }
}

void generateBool(llvm::raw_ostream &os, clang::QualType qt,
                  const std::string &indent, const std::string &self) {
  os << indent;
  if (!qt.getTypePtr()->isBooleanType()) {
    os << return_false;
  } else {
    os << self << " = b;\n";
  }
}
void generateInt(llvm::raw_ostream &os, clang::QualType qt,
                 const std::string &indent, const std::string &self) {
  os << indent;
  if (!qt.getTypePtr()->isIntegerType()) {
    os << return_false;
  } else {
    os << self << " = i;\n";
  }
}
void generateUInt(llvm::raw_ostream &os, clang::QualType qt,
                  const std::string &indent, const std::string &self) {
  os << indent;
  if (!qt.getTypePtr()->isUnsignedIntegerType()) {
    os << return_false;
  } else {
    os << self << " = u;\n";
  }
}
// TODO: distinguish integer types more finely
void generateInt64(llvm::raw_ostream &os, clang::QualType qt,
                   const std::string &indent, const std::string &self) {
  return generateInt(os, qt, indent, self);
}
void generateUint64(llvm::raw_ostream &os, clang::QualType qt,
                    const std::string &indent, const std::string &self) {
  return generateUInt(os, qt, indent, self);
}
// TODO: distinguish floating types more finely is necessary
void generateDouble(llvm::raw_ostream &os, clang::QualType qt,
                    const std::string &indent, const std::string &self) {
  os << indent;
  if (!qt.getTypePtr()->isFloatingType()) {
    os << return_false;
  } else {
    os << self << " = d;\n";
  }
}
*/
} // namespace

/* INFO: name-mangling:
 * add VB to virtual base class name
 * add B to base class name
 * add M to data member name
 * append a '_' to each level of name
 */
template <bool VisitBase, bool VisitVBase, bool VisitField, typename CB>
bool RecordInfo::Visit(const CodegenContext &cc, CB &cb) {
  auto visit_base = [&](const char *str, std::vector<SubClass> &bs) -> bool {
    for (SubClass &b : bs) {
      if (b.omit) {
        continue;
      }
      const clang::RecordType *type =
          llvm::dyn_cast<clang::RecordType>(b.base->getType());
      const clang::CXXRecordDecl *decl =
          llvm::dyn_cast<clang::CXXRecordDecl>(type->getDecl());
      std::string vbase_name = decl->getName().str();
      std::string self = "static_cast<" + vbase_name + ">(" + cc.self + ")";
      std::string prefix = cc.prefix + str + vbase_name + "_";
      CodegenContext cc1;
      cc1.indent = cc.indent;
      cc1.self = self;
      cc1.start_state = cc.start_state;
      ;
      cc1.expact_key_state = cc.expact_key_state;
      cc1.prefix = prefix;
      RecordInfo *ri = getRecordInfoFromDecl(decl);
      if (!ri->Visit(cc1, cb)) {
        return false;
      }
    }
    return true;
  };
  if (VisitVBase) {
    // visit all virtual base
    if (!visit_base("VB", vbases)) {
      return false;
    }
  }
  if (VisitBase) {
    // visit all bases
    if (!visit_base("B", bases)) {
      return false;
    }
  }
  if (VisitField) {
    // visit all direct field
    for (Field &f : fields) {
      if (f.directive.is_omit) {
        continue;
      }
      if (f.field->isUnnamedBitfield()) {
        // TODO:
        abort();
      }
      VisitContext vc;
      std::string field_name = f.field->getName().str();
      vc.state_name = cc.prefix + "M" + field_name;
      vc.parent = cc.self;
      vc.self = cc.self + "." + field_name;
      if (!cb(vc, f)) {
        return false;
      }
    }
  }
  return true;
}

bool RecordInfo::generateEnumBody(llvm::raw_ostream &os,
                                  const CodegenContext &cc) {
  auto cb = [&](const VisitContext &vc, const Field &) {
    os << cc.indent << vc.state_name << '\n';
    return true;
  };
  return Visit(cc, cb);
}

bool RecordInfo::generateNullBody(llvm::raw_ostream &os,
                                  const CodegenContext &cc) {
  auto cb = [&](const VisitContext &vc, const Field &f) -> bool {
    os << cc.indent << "case " << vc.state_name << ":\n";
    const clang::Type *type = f.field->getType().getTypePtr();
    if (type->isIntegerType() || type->isFloatingType()) {
      os << cc.indent << vc.self << " = 0;\n";
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else if (type->isPointerType()) {
      os << cc.indent << vc.self << " = nullptr;\n";
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else {
      os << cc.indent << return_false;
    }
    return true;
  };
  return Visit(cc, cb);
}

bool RecordInfo::generateBoolBody(llvm::raw_ostream &os,
                                  const CodegenContext &cc) {
  auto cb = [&](const VisitContext &vc, const Field &f) -> bool {
    os << cc.indent << "case " << vc.state_name << ":\n";
    const clang::Type *type = f.field->getType().getTypePtr();
    if (type->isBooleanType()) {
      os << cc.indent << vc.self << " = b;\n";
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else {
      os << cc.indent << return_false;
    }
    return true;
  };
  return Visit(cc, cb);
}

// TODO: handle enum as integer

bool RecordInfo::generateIntBody(llvm::raw_ostream &os,
                                 const CodegenContext &cc) {
  auto cb = [&](const VisitContext &vc, const Field &f) -> bool {
    os << cc.indent << "case " << vc.state_name << ":\n";
    const clang::Type *type = f.field->getType().getTypePtr();
    if (type->isIntegerType()) {
      os << cc.indent << vc.self << " = i;\n";
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else {
      os << cc.indent << return_false;
    }
    return true;
  };
  return Visit(cc, cb);
}

bool RecordInfo::generateUintBody(llvm::raw_ostream &os,
                                  const CodegenContext &cc) {
  auto cb = [&](const VisitContext &vc, const Field &f) -> bool {
    os << cc.indent << "case " << vc.state_name << ":\n";
    const clang::Type *type = f.field->getType().getTypePtr();
    if (type->isUnsignedIntegerOrEnumerationType()) {
      os << cc.indent << vc.self << " = u;\n";
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else {
      os << cc.indent << return_false;
    }
    return true;
  };
  return Visit(cc, cb);
}

// TODO: handle integer more finely
bool RecordInfo::generateInt64Body(llvm::raw_ostream &os,
                                   const CodegenContext &cc) {
  auto cb = [&](const VisitContext &vc, const Field &f) -> bool {
    os << cc.indent << "case " << vc.state_name << ":\n";
    const clang::Type *type = f.field->getType().getTypePtr();
    if (type->isIntegerType()) {
      os << cc.indent << vc.self << " = i;\n";
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else {
      os << cc.indent << return_false;
    }
    return true;
  };
  return Visit(cc, cb);
}

bool RecordInfo::generateUint64Body(llvm::raw_ostream &os,
                                    const CodegenContext &cc) {
  auto cb = [&](const VisitContext &vc, const Field &f) -> bool {
    os << cc.indent << "case " << vc.state_name << ":\n";
    const clang::Type *type = f.field->getType().getTypePtr();
    if (type->isUnsignedIntegerOrEnumerationType()) {
      os << cc.indent << vc.self << " = u;\n";
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else {
      os << cc.indent << return_false;
    }
    return true;
  };
  return Visit(cc, cb);
}

// TODO: handle floating-point type more finely
bool RecordInfo::generateDoubleBody(llvm::raw_ostream &os,
                                    const CodegenContext &cc) {
  auto cb = [&](const VisitContext &vc, const Field &f) -> bool {
    os << cc.indent << "case " << vc.state_name << ":\n";
    const clang::Type *type = f.field->getType().getTypePtr();
    if (type->isFloatingType()) {
      os << cc.indent << vc.self << " = u;\n";
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else {
      os << cc.indent << return_false;
    }
    return true;
  };
  return Visit(cc, cb);
}

// TODO: handle char array
// TODO: char8/char16/char32
// note: only support in-site parse, that copy is guaranteed to be false
// note: only support utf-8
bool RecordInfo::generateStringBody(llvm::raw_ostream & os,
                                    const CodegenContext & cc) {
  auto cb = [&](const VisitContext &vc, const Field &f) -> bool {
    os << cc.indent << "case " << vc.state_name << ":\n";
    if (f.directive.is_c_string) {
      // note: self is a non-owning pointer
      // note: str's content may contains NULL, that is strlen(str) <= length
      os << cc.indent << vc.self << " = str;\n";
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else if (f.directive.is_string_pointer) {
      os << cc.indent  << vc.self << " = str;\n";
      os << cc.indent << vc.parent << '.' << f.directive.param << " = length;\n";
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else if (f.directive.is_user_defined_string) {
      os << cc.indent << substituteDoubleDollar(f.directive.param, vc.self) << '\n';
      emitFieldCheck(os, cc, vc, f);
      os << cc.indent << return_true;
    } else {
      os << cc.indent << return_false;
    }
    return true;
  };
  if (!Visit(cc, cb)) {
    return false;
  }
  return true;
}

bool RecordInfo::generateKeyBody(llvm::raw_ostream &, const CodegenContext &) {
  return true;
}

bool RecordInfo::generateStartArrayBody(llvm::raw_ostream &,
                                        const CodegenContext &) {
  return true;
}
bool RecordInfo::generateEndArrayBody(llvm::raw_ostream &,
                                      const CodegenContext &) {
  return true;
}

// TODO: handle raw number
bool RecordInfo::generateRawNumberBody(llvm::raw_ostream &,
                                       const CodegenContext &) {
  return false;
}

void RecordInfo::emitCode(llvm::raw_ostream &) { return; }

