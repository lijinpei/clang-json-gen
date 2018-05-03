#pragma once

#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace clang {
namespace comments {
class FullComment;
}
} // namespace clang

struct RecordDirective {
  bool is_empty : 1;
  bool is_check_specified : 1;
  std::vector<std::string> omit_base;
  std::vector<std::pair<std::string, std::string>> named_base;
  RecordDirective(const clang::comments::FullComment *);
  void Dump(llvm::raw_ostream & os) {
    if (is_empty) {
      os << "empty";
      return;
    }
    if (is_check_specified) {
      os << "check_specified ";
    }
    os << "omit_base: ";
    for (const auto & b : omit_base) {
      os << b << ' ';
    }
    os << "named_base: ";
    for (const auto & nb: named_base) {
      os << nb.first << ':' << nb.second << ' ';
    }
  }
};

struct FieldDirective {
  bool is_empty : 1;
  bool is_required : 1;
  bool is_omit : 1;

  bool is_c_string : 1;
  bool is_string_pointer : 1;
  bool is_string_length : 1;
  bool is_user_defined_string;

  bool is_null_terminated_array : 1;
  bool is_array_pointer : 1;
  bool is_array_length : 1;
  bool is_user_defined_array : 1;

  // the meaning of this string depends on the previous bitfields
  std::string param;

  FieldDirective(const clang::comments::FullComment *);
  void Dump(llvm::raw_ostream & os) {
    if (is_empty) {
      os << "empty";
      return;
    }
    if (is_required) {
      os << "required ";
    }
    if (is_omit) {
      os << "omit ";
    }
    if (is_c_string) {
      os << "c-style string";
      return;
    }
    if (is_string_pointer) {
      os << "pointer to char with " << param;
      return;
    }
    if (is_string_length) {
      os << "length of string";
      return;
    }
    if (is_user_defined_string) {
      os << "user defined string with " << param;
      return;
    }
    if (is_null_terminated_array) {
      os << "null-terminated array";
      return;
    }
    if (is_array_pointer) {
      os << "array pointer with " << param;
      return;
    }
    if (is_array_length) {
      os << "array length";
      return;
    }
    if (is_user_defined_array) {
      os << "user defined array with " << param;
    }
    os << "wrong field directive: not empty but no content";
  }
};
