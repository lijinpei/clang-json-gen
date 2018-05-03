#pragma once

#include "JsonGen.hpp"

#include "clang/AST/Comment.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "clang/Basic/Diagnostic.h"

#include <memory>
#include <ostream>

namespace clang {
class ASTContext;
}

class RecordInfo;

/* The class we generate code for must satisfy the following conditions:
 * 1 each direct base is not also inherited as a indirect base(otherwise we
 * won't be able to convert the class to it's direct base to access the direct
 * base 2 only generate code for members that is accessible to this class
 */
class JsonGenTypeVisitor {
  std::ostream &os;
  clang::ASTContext *ast_context;
  clang::DiagnosticEngine *diag;
  unsigned diag_error_complex, diag_error_block_pointer,
      diag_error_incomplete_array, diag_error_vla, diag_error_template,
      diag_error_simd, diag_error_attributed_type,
      diag_error_injected_class_name, diag_error_objc, diag_error_pipe,
      diag_error_atomic;
  unsigned diag_warning_pointer_as_integer,
      diag_warning_function_proto_as_integer,
      diag_warning_function_no_proto_as_integer, diag_warning_paren_as_integer,
      diag_warning_enum_as_int64_t;
  llvm::DenseMap<const clang::CXXRecordDecl *, RecordInfo *> record_infos;

  // QualType is not part of the clang Type system, but we provide it here as a
  // convenient helper
  bool Visit(clang::QualType qt) { return Visit(qt.getTypePtr()); }
  bool Visit(const clang::Type *T) {
    // Top switch stmt: dispatch to VisitFooType for each FooType.
#define DISPATCH(CLASS)                                                        \
  return Visit##CLASS(static_cast<const clang::CLASS *>(T))
#define ABSTRACT_TYPE(CLASS, PARENT)
#define TYPE(CLASS, PARENT)                                                    \
  case clang::Type::CLASS:                                                     \
    DISPATCH(CLASS##Type);
    switch (T->getTypeClass()) {
#include "clang/AST/TypeNodes.def"
#undef DISPATCH
#undef ABSTRACT_TYPE
#undef TYPE
    }
    llvm_unreachable("Unknown type class!");
  }

  bool VisitBuiltinType(const clang::BuiltinType *builtin) {
    SPDLOG_ENTER();
    return true;
  }

  bool VisitComplexType(const clang::ComplexType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_complex);
    return false;
  }
  bool VisitPointerType(const clang::PointerType *) {
    SPDLOG_ENTER();
    diags->Report(diag_warning_pointer_as_integer);
    // treate pointer as uint_t
    return false;
  }
  bool VisitBlockPointerType(const clang::BlockPointerType *) {
    SPDLOG_ENTER();
    // I don't quite know what is a block pointer type, but I think I can return
    // false here
    diags->Report(diag_error_block_pointer);
    return false;
  }
  bool VisitLValueReferenceType(const clang::LValueReferenceType *lref) {
    SPDLOG_ENTER();
    return Visit(lref->getPointeeType());
  }
  bool VisitRValueReferenceType(const clang::RValueReferenceType *rref) {
    SPDLOG_ENTER();
    return Visit(rref->getPointeeType());
  }
  bool VisitMemberPointerType(const clang::MemberPointerType) {
    SPDLOG_ENTER();
    // treate member pointer as integer
    return true;
  }
  bool VisitConstantArrayType(const clang::ConstantArrayType *carr) {
    SPDLOG_ENTER();
    return Visit(carr->getElementType());
  }
  bool VisitIncompleteArrayType(const clang::IncompleteArrayType *iarr) {
    SPDLOG_ENTER();
    // TODO: add support for incomplete array
    // TODO: add source location
    diags->Report(diag_error_incomplete_array);
    return false;
  }
  bool VisitVariableArrayType(const clang::VariableArrayType *varr) {
    SPDLOG_ENTER();
    // TODO: add support for vla
    // TODO: add source location
    diags->Report(diag_error_vla);
    return false;
  }
  bool
  VisitDependentSizedArrayType(const clang::DependentSizedArrayType *asarr) {
    SPDLOG_ENTER();
    // no support for template
    diags->Report(diag_error_template);
    return false;
  }
  bool
  VisitDependentSizedExtVectorType(const clang::DependentSizedExtVectorType *) {
    SPDLOG_ENTER();
    // only appears in templates
    // and see comment in VisitVectorType()
    diags->Report(diag_error_template);
    diags->Report(diag_error_simd);
    return false;
  }
  bool
  VisitDependentAddressSpaceType(const clang::DependentAddressSpaceType *dass) {
    SPDLOG_ENTER();
    // only appears in templates
    diags->Report(diag_error_template);
    return false;
  }
  bool VisitVectorType(const clang::VectorType *) {
    SPDLOG_ENTER();
    // gcc's simd extension:
    // https://gcc.gnu.org/onlinedocs/gcc/Vector-Extensions.html
    // no support yet
    diags->Report(diag_error_simd);
    return false;
  }
  bool VisitExtVectorType(const clang::ExtVectorType *) {
    SPDLOG_ENTER();
    // see the comment in VisitVectorType
    diags->Report(diag_error_simd);
    return false;
  }
  bool VisitFunctionProtoType(const clang::FunctionProtoType *) {
    SPDLOG_ENTER();
    // treate as integer
    diags->Report(diag_warning_function_proto_as_integer);
    return true;
  }
  bool VisitFunctionNoProtoType(const clang::FunctionNoProtoType *) {
    SPDLOG_ENTER();
    // treate as integer
    diags->Report(diag_warning_function_no_proto_as_integer);
    return true;
  }
  bool VisitUnresolvedUsingType(const clang::UnresolvedUsingType *) {
    SPDLOG_ENTER();
    // no support for template yet
    diags->Report(diag_error_template);
    return false;
  }
  bool VisitParenType(const clang::ParenType *) {
    SPDLOG_ENTER();
    // treate as integer
    diags->Report(diag_warning_paren_as_integer);
    return true;
  }
  bool VisitTypedefType(const clang::TypedefType *t, ) {
    SPDLOG_ENTER();
    // desugar() and pray for it to be correct
    return Visit(t->desugar());
  }
  bool VisitAdjustedType(const clang::AdjustedType *t) {
    SPDLOG_ENTER();
    // desugar() and pray for it to be correct
    return Visit(t->desugar());
  }
  bool VisitDecayedType(const clang::DecayedType *t) {
    SPDLOG_ENTER();
    // desugar() and pray for it to be correct
    return Visit(t->desugar());
  }
  bool VisitTypeOfExprType(const clang::TypeOfExprType *t) {
    SPDLOG_ENTER();
    // desugar() and pray for it to be correct
    return Visit(t->desugar());
  }
  bool VisitTypeOfType(const clang::TypeOfType *t) {
    SPDLOG_ENTER();
    // desugar() and pray for it to be correct
    return Visit(t->desugar());
  }
  bool VisitDecltypeType(const clang::DecltypeType *t) {
    SPDLOG_ENTER();
    // desugar() and pray for it to be correct
    return Visit(t->desugar());
  }
  bool VisitUnaryTransformType(const clang::UnaryTransformType *t) {
    SPDLOG_ENTER();
    // desugar() and pray for it to be correct
    return Visit(t->desugar());
  }
  bool VisitRecordType(const clang::RecordType *t);
  bool VisitEnumType(const clang::EnumType *t) {
    SPDLOG_ENTER();
    // treate enum as int64_t, and pray for it not to blow up
    diags->Report(diag_warning_enum_as_int64_t);
    return true;
  }
  bool VisitElaboratedType(const clang::ElaboratedType *t) {
    SPDLOG_ENTER();
    // elaborated type specifier can only appear in templates
    SPDLOG_ERROR(console_logger, "no support for template related type");
    diags->Report(diag_error_template);
    return false;
  }
  bool VisitAttributedType(const clang::AttributedType *t) {
    SPDLOG_ENTER();
    // return false because we don't understand all the attributes
    diags->Repoer(diag_error_attributed_type);
    return false;
  }
  bool VisitTemplateTypeParmType(const clang::TemplateTypeParmType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_template);
    return false;
  }
  bool
  VisitSubstTemplateTypeParmType(const clang::SubstTemplateTypeParmType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_template);
    return false;
  }
  bool VisitSubstTemplateTypeParmPackType(
      const clang::SubstTemplateTypeParmPackType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_template);
    return false;
  }
  bool
  VisitTemplateSpecializationType(const clang::TemplateSpecializationType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_template);
    return false;
  }
  bool VisitAutoType(const clang::AutoType *t) {
    return Visit(t->getDeducedType());
  }
  bool VisitDeducedTemplateSpecializationType(
      const clang::DeducedTemplateSpecializationType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_template);
    return false;
  }
  bool VisitInjectedClassNameType(const clang::InjectedClassNameType *) {
    SPDLOG_ENTER();
    // return false because you can not create recursive object in json
    // unless you use features like option, but we don't support that yet
    diags->Report(diag_error_template);
    return false;
  }
  bool VisitDependentNameType(const clang::DependentNameType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_template);
    return false;
  }

  bool VisitDependentTemplateSpecializationType(
      const clang::DependentTemplateSpecializationType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_template);
    return false;
  }
  bool VisitPackExpansionType(const clang::PackExpansionType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_template);
    return false;
  }
  bool VisitObjCTypeParamType(const clang::ObjCTypeParamType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_objc);
    return false;
  }
  bool VisitObjCObjectType(const clang::ObjCObjectType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_objc);
    return false;
  }
  bool VisitObjCInterfaceType(const clang::ObjCInterfaceType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_objc);
    return false;
  }
  bool VisitObjCObjectPointerType(const clang::ObjCObjectPointerType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_objc);
    return false;
  }
  bool VisitPipeType(const clang::PipeType *) {
    SPDLOG_ENTER();
    diags->Report(diag_error_pipe);
    return false;
  }
  bool VisitAtomicType(const clang::AtomicType *t) {
    SPDLOG_ENTER();
    diags->Report(diag_error_atomic);
    return false;
  }

public:
  JsonGenTypeVisitor(std::ostream &, clang::ASTContext *);
  ~JsonGenTypeVisitor() {
    for (auto &kv : record_infos) {
      delete kv.second;
    }
  }
  // generate code for a CXXRecordDecl
  const RecordInfo* addRecord(clang::CXXRecordDecl *decl,
                 const clang::comments::FullComment *fc) {
    RecordDirective rd(fc);
    if (rd.empty()) {
      return nullptr;
    }
    if (const auto * ri = record_infos[decl]) {
      return ri;
    }
    Visit(decl->getTypeForDecl());
    auto & ri = record_infos[decl];
    ri.setDirective(rd);
    return ri;
  }
  const RecordInfo *getInfo(clang::CXXRecordDecl *decl) {
  }
};
