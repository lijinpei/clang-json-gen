#include "Directive.hpp"

#include "clang/AST/Comment.h"
#include "clang/AST/CommentVisitor.h"

/*
 * supported commands:
 * RecordDirective:
 * \jsongen generate json parser for this class
 *
 * \omitBase <a list of base class names till next command>, don't generate code
 * for the specified bases
 *
 * FieldDirective:
 * \required this is a required field, if it is not present, bool valid()
 * returns false
 *
 * \omit don't parse this field
 *
 * \cstring this pointer to char should be treated as a c-style
 * string(null-terminated char array)
 *
 * \usrString expression-statement, this command specified that the memeber
 * should be treated as a string, is takes as parameter a expression-statement
 * till the next command or end of comment(you should include the comma at the
 * end of your specified statement). The statement is used to construct/assign
 * the memeber when needed, in the statement, use $$ to denotes the member,
 * str/lenght/copy are the parameter of rapidJson's String() function, for
 * example:
 *
 * struct A {
 *   std::string s; /// \usrString $$ = std::string(str);
 * };
 *
 * \string var, specify this member should be treated as a string. This member
 * should be a pointer to char, and var should be be a integer type. The
 * generated code will store the str pointer to this member, and length to var,
 * your class owns the pointed address(i.e. your class should free it using
 * free()); This command will cause var to be treated as \omit. Note that the
 * string provided by rapidJson is not null-terminated. Example:
 *
 * struct B {
 *  const char * str; /// \string length
 *  int length;
 * };
 *
 * \nullArray this member is a pointer to null-terminated array
 *
 * \usrArry expresssion-statement, like \usrArray
 *
 * \array var, like \string
 */

RecordDirective::RecordDirective(const clang::comments::FullComment*) {
  is_empty = true;
}

FieldDirective::FieldDirective(const clang::comments::FullComment*) {
  is_empty = true;
}

