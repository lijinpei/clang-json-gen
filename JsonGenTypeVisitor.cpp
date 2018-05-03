#include "JsonGenTypeVisitor.hpp"

#include "clang/Basic/Diagnostic.h"

RecordDirective::RecordDirective(const clang::comments::FullComment* fc) {
}

FiedlDirective::FiedlDirective(const clang::comments::FullComment* fc) {
}

JsonGenTypeVisitor::JsonGenTypeVisitor(std::ostream& os, clang::ASTContext *ast_context)
    : os(os), ast_context(ast_context), diags(ast_context->getDiagnostics()) {
  diag_error_complex = diags->getCustomDiagID(clang::DiagnosticEngine::Error,
                                              "no support for complex type");
  diag_error_block_pointer = diags->getCustomDiagID(
      clang::DiagnosticEngine::Error, "no support for block pointer");
  diag_error_incomplete_array = diags->getCustomDiagID(
      clang::DiagnosticEngine::Error, "no support for incomplete array");
  diag_error_vla = diags->getCustomDiagID(
      clang::DiagnosticEngine::Error, "no support for variable length array");
  diag_error_template = diags->getCustomDiagID(clang::DiagnosticEngine::Error,
                                               "no support for template");
  diag_error_simd = diags->getCustomDiagID(
      clang::DiagnosticEngine::Error, "no support for gnu vector extension");
  diag_error_attributed_type = diags->getCustomDiagID(
      clang::DiagnosticEngine::Error, "no support for attributed type");
  diag_error_injected_class_name = diags->getCustomDiagID(
      clang::DiagnosticEngine::Error, "can not have injected class name");
  diag_error_objc = diags->getCustomDiagID(clang::DiagnosticEngine::Error,
                                           "no support for objc type");
  diag_error_pipe = diags->getCustomDiagID(clang::DiagnosticEngine::Error,
                                           "no support for OpenCL Pipe");
  diag_error_atomic = diags->getCustomDiagID(clang::DiagnosticEngine::Error,
                                             "no support for atomic type");
  diag_warning_pointer_as_integer = diags->getCustomDiagID(
      clang::DiagnosticEngine::Warning, "treating pointer as integer");
  diag_warning_function_proto_as_integer = diags->getCustomDiagID(
      clang::DiagnosticEngine::Warning, "treating function proto as integer");
  diag_warning_function_no_proto_as_integer =
      diags->getCustomDiagID(clang::DiagnosticEngine::Warning,
                             "treating FunctionNoProtoType as integer");
  diag_warning_paren_as_integer = diags->getCustomDiagID(
      clang::DiagnosticEngine::Warning, "treating ParenType as integer");
  diag_warning_enum_as_int64_t = diags->getCustomDiagID(
      clang::DiagnosticEngine::Warning, "treating enum as int64_t");
}

bool JsonGenTypeVisitor::generateCode(clang::CXXRecordDecl * reco,
                            const clang::comments::FullComment * fc) {
  if (RecordInfo * ri = record_infos[reco]) {
    return generateCode_impl(ri);
  }
  visited_records.clear();
  visited_records.insert(reco);
  Visit(decl->getTypeForDecl());
  return generateCode_impl(record_infos[reco]);
}

bool JsonGenTypeVisitor::VisitRecordType(const clang::RecordType * reco_) {
  const clang::CXXRecordDecl* reco = static_cast<const clang::CXXRecordDecl*>(reco_);
  if (!reco) {
    return false;
  }
  if (record_infos[reco]) {
    return true;
  }
  if (visited_records[reco]) {
    return false;
  }
  RecordInfo * ri = new RecordInfo;
  for (const clang::FieldDecl* fd : reco->fields()) {
    const clang::Type * type = fd->getType().getTypePtr();
    if (!Visit(type)) {
      return false;
    }
    ri->addMember(fd->getName().str(), type);
  }
  if (reco->beses_begin() == reco->bases_end()) {
    record_infos[reco] = ri;
    return true;
  }
  visited_records.insert(reco);
  for (const CXXBaseSpecifier * bs : reco->bases()) {
    const clang::RecordType * type = staitc_cast<const clang::RecordType*>(bs->getType().getTypePtr());
    if (!Visit(type)) {
      return false;
    }
    if (bs->isVirtual()) {
      ri->addVirtualBase(type);
    } else {
      ri->addBase(type);
    }
  }
  record_infos[reco] = ri;
  return true;
}
