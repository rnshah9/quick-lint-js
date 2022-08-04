// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quick-lint-js/array.h>
#include <quick-lint-js/cli/cli-location.h>
#include <quick-lint-js/container/padded-string.h>
#include <quick-lint-js/diag-collector.h>
#include <quick-lint-js/diag-matcher.h>
#include <quick-lint-js/fe/diagnostic-types.h>
#include <quick-lint-js/fe/language.h>
#include <quick-lint-js/fe/parse.h>
#include <quick-lint-js/parse-support.h>
#include <quick-lint-js/port/char8.h>
#include <quick-lint-js/spy-visitor.h>
#include <string>
#include <string_view>
#include <vector>

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace quick_lint_js {
namespace {
class test_parse_module : public test_parse_expression {};

TEST_F(test_parse_module, export_variable) {
  {
    test_parser p(u8"export let x;"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration"));
    EXPECT_THAT(p.variable_declarations, ElementsAre(let_noinit_decl(u8"x")));
  }

  {
    test_parser p(u8"export let x = 42;"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration"));
    EXPECT_THAT(p.variable_declarations, ElementsAre(let_init_decl(u8"x")));
  }

  {
    test_parser p(u8"export var x;"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration"));
    EXPECT_THAT(p.variable_declarations, ElementsAre(var_noinit_decl(u8"x")));
  }

  {
    test_parser p(u8"export var x = 42;"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration"));
    EXPECT_THAT(p.variable_declarations, ElementsAre(var_init_decl(u8"x")));
  }

  {
    test_parser p(u8"export const x = null;"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration"));
    EXPECT_THAT(p.variable_declarations, ElementsAre(const_init_decl(u8"x")));
  }
}

TEST_F(test_parse_module, export_default) {
  {
    test_parser p(u8"export default x;"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_use"));
  }

  {
    test_parser p(u8"export default function f() {}"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration",       // f
                                      "visit_enter_function_scope",       //
                                      "visit_enter_function_scope_body",  //
                                      "visit_exit_function_scope"));
  }

  {
    test_parser p(u8"export default function() {}"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_function_scope",       //
                                      "visit_enter_function_scope_body",  //
                                      "visit_exit_function_scope"));
  }

  {
    test_parser p(u8"export default async function f() {}"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration",       // f
                                      "visit_enter_function_scope",       //
                                      "visit_enter_function_scope_body",  //
                                      "visit_exit_function_scope"));
  }

  {
    test_parser p(u8"export default async function() {}"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_function_scope",       //
                                      "visit_enter_function_scope_body",  //
                                      "visit_exit_function_scope"));
  }

  {
    test_parser p(u8"export default (function f() {})"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_named_function_scope",  // f
                                      "visit_enter_function_scope_body",   //
                                      "visit_exit_function_scope"));
  }

  {
    test_parser p(u8"export default class C {}"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_class_scope",       //
                                      "visit_enter_class_scope_body",  //
                                      "visit_exit_class_scope",
                                      "visit_variable_declaration"));  // C
  }

  {
    test_parser p(u8"export default class {}"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_class_scope",       //
                                      "visit_enter_class_scope_body",  //
                                      "visit_exit_class_scope"));
  }

  {
    test_parser p(u8"export default async (a) => b;"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_function_scope",  //
                                      "visit_variable_declaration",  // a
                                      "visit_enter_function_scope_body",
                                      "visit_variable_use",  // b
                                      "visit_exit_function_scope"));
  }
}

TEST_F(test_parse_module, export_default_of_variable_is_illegal) {
  for (string8 declaration_kind : {u8"const", u8"let", u8"var"}) {
    string8 code = u8"export default " + declaration_kind + u8" x = y;";
    SCOPED_TRACE(out_string8(code));
    test_parser p(code, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_use",            // y
                                      "visit_variable_declaration"));  // x
    EXPECT_THAT(p.errors, ElementsAre(DIAG_TYPE_OFFSETS(
                              p.code, diag_cannot_export_default_variable,  //
                              declaring_token, strlen(u8"export default "),
                              declaration_kind)));
  }
}

TEST_F(test_parse_module, export_sometimes_requires_semicolon) {
  {
    test_parser p(u8"export {x} console.log();"_sv, capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_export_use",  // x
                                      "visit_variable_use",         // console
                                      "visit_end_of_module"));
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_missing_semicolon_after_statement,  //
                    where, strlen(u8"export {x}"), u8"")));
  }

  {
    test_parser p(u8"export * from 'other' console.log();"_sv, capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_use",  // console
                                      "visit_end_of_module"));
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_missing_semicolon_after_statement,  //
                    where, strlen(u8"export * from 'other'"), u8"")));
  }

  {
    test_parser p(u8"export default x+y console.log();"_sv, capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_use",  // x
                                      "visit_variable_use",  // y
                                      "visit_variable_use",  // console
                                      "visit_end_of_module"));
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_missing_semicolon_after_statement,  //
                    where, strlen(u8"export default x+y"), u8"")));
  }

  {
    test_parser p(u8"export default async () => {} console.log();"_sv,
                  capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_function_scope",       //
                                      "visit_enter_function_scope_body",  //
                                      "visit_exit_function_scope",        //
                                      "visit_variable_use",  // console
                                      "visit_end_of_module"));
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_missing_semicolon_after_statement,  //
                    where, strlen(u8"export default async () => {}"), u8"")));
  }
}

TEST_F(test_parse_module, export_sometimes_does_not_require_semicolon) {
  {
    test_parser p(u8"export default async function f() {} console.log();"_sv,
                  capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration",       // f
                                      "visit_enter_function_scope",       //
                                      "visit_enter_function_scope_body",  //
                                      "visit_exit_function_scope",        //
                                      "visit_variable_use",  // console
                                      "visit_end_of_module"));
    EXPECT_THAT(p.errors, IsEmpty());
  }

  {
    test_parser p(u8"export default function() {} console.log();"_sv,
                  capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_function_scope",       //
                                      "visit_enter_function_scope_body",  //
                                      "visit_exit_function_scope",        //
                                      "visit_variable_use",  // console
                                      "visit_end_of_module"));
    EXPECT_THAT(p.errors, IsEmpty());
  }
}

TEST_F(test_parse_module, export_list) {
  {
    test_parser p(u8"export {one, two};"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_export_use",    // one
                                      "visit_variable_export_use"));  // two
  }

  {
    test_parser p(u8"export {one as two, three as four};"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_export_use",    // one
                                      "visit_variable_export_use"));  // three
    EXPECT_THAT(p.variable_uses, ElementsAre(u8"one", u8"three"));
  }

  {
    test_parser p(u8"export {myVar as 'name'};"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.variable_uses, ElementsAre(u8"myVar"));
  }
}

TEST_F(test_parse_module,
       exporting_by_string_name_is_only_allowed_for_export_from) {
  {
    test_parser p(u8"export {'name'};"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code,
                    diag_exporting_string_name_only_allowed_for_export_from,  //
                    export_name, strlen(u8"export {"), u8"'name'")));
  }
}

TEST_F(test_parse_module,
       exported_variables_cannot_be_named_reserved_keywords) {
  for (string8 keyword : strict_reserved_keywords) {
    string8 code = u8"export {" + keyword + u8"};";
    SCOPED_TRACE(out_string8(code));
    test_parser p(code, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
    EXPECT_THAT(p.variable_uses, IsEmpty());
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_cannot_export_variable_named_keyword,  //
                    export_name, strlen(u8"export {"), keyword)));
  }

  for (string8 keyword : strict_reserved_keywords) {
    string8 code = u8"export {" + keyword + u8" as thing};";
    SCOPED_TRACE(out_string8(code));
    test_parser p(code, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
    EXPECT_THAT(p.variable_uses, IsEmpty());
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_cannot_export_variable_named_keyword,  //
                    export_name, strlen(u8"export {"), keyword)));
  }

  // TODO(strager): Test u8"await" and u8"yield".
  // TODO(#73): Disallow 'protected', 'implements', etc.
  for (string8 keyword : disallowed_binding_identifier_keywords) {
    string8 exported_variable = escape_first_character_in_keyword(keyword);

    {
      string8 code = u8"export {" + exported_variable + u8"};";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.variable_uses, IsEmpty());
      EXPECT_THAT(p.errors,
                  ElementsAre(DIAG_TYPE_OFFSETS(
                      p.code, diag_keywords_cannot_contain_escape_sequences,  //
                      escape_sequence, strlen(u8"export {"), u8"\\u{??}")));
    }

    {
      string8 code = u8"export {" + exported_variable + u8" as thing};";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.variable_uses, IsEmpty());
      EXPECT_THAT(p.errors,
                  ElementsAre(DIAG_TYPE_OFFSETS(
                      p.code, diag_keywords_cannot_contain_escape_sequences,  //
                      escape_sequence, strlen(u8"export {"), u8"\\u{??}")));
    }
  }
}

TEST_F(test_parse_module, export_from) {
  {
    test_parser p(u8"export * from 'other';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"export * as mother from 'other';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"export * as 'mother' from 'other';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"export {} from 'other';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"export {util1, util2, util3} from 'other';");
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"export {readFileSync as readFile} from 'fs';");
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"export {promises as default} from 'fs';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  for (string8 keyword : keywords) {
    padded_string code(u8"export {" + keyword + u8"} from 'other';");
    SCOPED_TRACE(code);
    test_parser p(code.string_view());
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    // Keywords are legal, even if Unicode-escaped.
    test_parser p(u8"export {\\u{76}ar} from 'fs';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    // Keywords are legal, even if Unicode-escaped.
    test_parser p(u8"export {\\u{76}ar as \\u{69}f} from 'fs';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"export {'name'} from 'other';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"export {'name' as 'othername'} from 'other';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }
}

TEST_F(test_parse_module, invalid_export_expression) {
  {
    test_parser p(u8"export stuff;"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors, ElementsAre(DIAG_TYPE_OFFSETS(
                              p.code, diag_exporting_requires_curlies,  //
                              names, strlen(u8"export "), u8"stuff")));
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_use"));  // stuff
  }

  {
    test_parser p(u8"export a, b, c;"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(
        p.errors,
        // TODO(strager): Report diag_exporting_requires_curlies instead.
        ElementsAre(
            DIAG_TYPE_OFFSETS(p.code, diag_exporting_requires_default,  //
                              expression, strlen(u8"export "), u8"a, b, c")));
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_use",    // a
                                      "visit_variable_use",    // b
                                      "visit_variable_use"));  // c
  }

  {
    test_parser p(u8"export a, b, c+d;"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors,
                // TODO(strager): Should we report
                // diag_exporting_requires_curlies instead?
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_exporting_requires_default,  //
                    expression, strlen(u8"export "), u8"a, b, c+d")));
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_use",    // a
                                      "visit_variable_use",    // b
                                      "visit_variable_use",    // c
                                      "visit_variable_use"));  // d
  }

  {
    test_parser p(u8"export 2 + x;"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors, ElementsAre(DIAG_TYPE_OFFSETS(
                              p.code, diag_exporting_requires_default,  //
                              expression, strlen(u8"export "), u8"2 + x")));
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_use"));  // x
  }
}

TEST_F(test_parse_module, invalid_export) {
  {
    test_parser p(u8"export ;"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors, ElementsAre(DIAG_TYPE_OFFSETS(
                              p.code, diag_missing_token_after_export,  //
                              export_token, 0, u8"export")));
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"export "_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors, ElementsAre(DIAG_TYPE_OFFSETS(
                              p.code, diag_missing_token_after_export,  //
                              export_token, 0, u8"export")));
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"export = x"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors, ElementsAre(DIAG_TYPE_OFFSETS(
                              p.code, diag_unexpected_token_after_export,  //
                              unexpected_token, strlen(u8"export "), u8"=")));
    p.parse_and_visit_statement();                             // Parse '= x'.
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_use"));  // x
  }
}

TEST_F(test_parse_module, parse_and_visit_import) {
  {
    test_parser p(u8"import 'foo';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, IsEmpty());
  }

  {
    test_parser p(u8"import fs from 'fs'"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.variable_declarations, ElementsAre(import_decl(u8"fs")));
  }

  {
    test_parser p(u8"import * as fs from 'fs'"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.variable_declarations, ElementsAre(import_decl(u8"fs")));
  }

  {
    test_parser p(u8"import fs from 'fs'; import net from 'net';"_sv,
                  capture_diags);
    p.parse_and_visit_statement();
    p.parse_and_visit_statement();
    EXPECT_THAT(p.variable_declarations,
                ElementsAre(import_decl(u8"fs"), import_decl(u8"net")));
    EXPECT_THAT(p.errors, IsEmpty());
  }

  {
    test_parser p(u8"import { readFile, writeFile } from 'fs';");
    p.parse_and_visit_statement();
    EXPECT_THAT(
        p.variable_declarations,
        ElementsAre(import_decl(u8"readFile"), import_decl(u8"writeFile")));
  }

  {
    test_parser p(u8"import {readFileSync as rf} from 'fs';"_sv);
    p.parse_and_visit_statement();
    ASSERT_EQ(p.variable_declarations.size(), 1);
    EXPECT_THAT(p.variable_declarations, ElementsAre(import_decl(u8"rf")));
  }

  {
    test_parser p(u8"import {'read file sync' as readFileSync} from 'fs';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.variable_declarations,
                ElementsAre(import_decl(u8"readFileSync")));
  }

  {
    test_parser p(u8"import fs, {readFileSync} from 'fs';"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(
        p.variable_declarations,
        ElementsAre(import_decl(u8"fs"), import_decl(u8"readFileSync")));
  }

  {
    test_parser p(u8"import fsDefault, * as fsExports from 'fs';");
    p.parse_and_visit_statement();
    EXPECT_THAT(
        p.variable_declarations,
        ElementsAre(import_decl(u8"fsDefault"), import_decl(u8"fsExports")));
  }
}

TEST_F(test_parse_module, import_star_without_as_keyword) {
  {
    test_parser p(u8"import * myExport from 'other';"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(
        p.errors,
        ElementsAre(DIAG_TYPE_3_OFFSETS(
            p.code,
            diag_expected_as_before_imported_namespace_alias,               //
            star_through_alias_token, strlen(u8"import "), u8"* myExport",  //
            star_token, strlen(u8"import "), u8"*",                         //
            alias, strlen(u8"import * "), u8"myExport")));
    EXPECT_THAT(p.visits,
                ElementsAre("visit_variable_declaration"));  // myExport
  }
}

TEST_F(test_parse_module, import_without_from_keyword) {
  {
    test_parser p(u8"import { x } 'other';"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_expected_from_before_module_specifier,  //
                    module_specifier, strlen(u8"import { x } "), u8"'other'")));
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration"));  // x
  }

  {
    test_parser p(u8"import { x } ;"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_expected_from_and_module_specifier,  //
                    where, strlen(u8"import { x }"), u8"")));
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration"));  // x
  }
}

TEST_F(test_parse_module, import_as_invalid_token) {
  {
    test_parser p(u8"import {myExport as 'string'} from 'module';"_sv,
                  capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(
        p.errors,
        ElementsAre(DIAG_TYPE_OFFSETS(
            p.code, diag_expected_variable_name_for_import_as,  //
            unexpected_token, strlen(u8"import {myExport as "), u8"'string'")));
  }

  {
    test_parser p(u8"import {'myExport' as 'string'} from 'module';"_sv,
                  capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_expected_variable_name_for_import_as,  //
                    unexpected_token, strlen(u8"import {'myExport' as "),
                    u8"'string'")));
  }
}

TEST_F(test_parse_module, export_function) {
  {
    test_parser p(u8"export function foo() {}"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.variable_declarations, ElementsAre(function_decl(u8"foo")));
  }

  {
    test_parser p(u8"export async function foo() {}"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.variable_declarations, ElementsAre(function_decl(u8"foo")));
  }
}

TEST_F(test_parse_module, export_function_requires_a_name) {
  {
    test_parser p(u8"export function() {}"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_function_scope",       //
                                      "visit_enter_function_scope_body",  //
                                      "visit_exit_function_scope"));
    EXPECT_THAT(p.errors,
                ElementsAre(DIAG_TYPE_OFFSETS(
                    p.code, diag_missing_name_of_exported_function,  //
                    function_keyword, strlen(u8"export "), u8"function")));
  }

  {
    test_parser p(u8"export async function() {}"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_function_scope",       //
                                      "visit_enter_function_scope_body",  //
                                      "visit_exit_function_scope"));
    EXPECT_THAT(
        p.errors,
        ElementsAre(DIAG_TYPE_OFFSETS(
            p.code, diag_missing_name_of_exported_function,  //
            function_keyword, strlen(u8"export async "), u8"function")));
  }
}

TEST_F(test_parse_module, export_class) {
  {
    test_parser p(u8"export class C {}"_sv);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.variable_declarations, ElementsAre(class_decl(u8"C")));
  }
}

TEST_F(test_parse_module, export_class_requires_a_name) {
  {
    test_parser p(u8"export class {}"_sv, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAre("visit_enter_class_scope",       //
                                      "visit_enter_class_scope_body",  //
                                      "visit_exit_class_scope"));
    EXPECT_THAT(p.errors, ElementsAre(DIAG_TYPE_OFFSETS(
                              p.code, diag_missing_name_of_exported_class,  //
                              class_keyword, strlen(u8"export "), u8"class")));
  }
}

TEST_F(test_parse_module, parse_empty_module) {
  test_parser p(u8""_sv, capture_diags);
  p.parse_and_visit_module();
  EXPECT_THAT(p.errors, IsEmpty());
  EXPECT_THAT(p.visits, ElementsAre("visit_end_of_module"));
}

TEST_F(test_parse_module, imported_variables_can_be_named_contextual_keywords) {
  for (string8 name : contextual_keywords - dirty_set<string8>{u8"let"}) {
    SCOPED_TRACE(out_string8(name));

    {
      test_parser p(u8"import { " + name + u8" } from 'other';");
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_declaration"));  // (name)
    }

    {
      test_parser p(u8"import { exportedName as " + name +
                    u8" } from 'other';");
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_declaration"));  // (name)
    }

    {
      test_parser p(u8"import { 'exportedName' as " + name +
                    u8" } from 'other';");
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_declaration"));  // (name)
    }

    {
      test_parser p(u8"import " + name + u8" from 'other';");
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_declaration"));  // (name)
    }

    {
      test_parser p(u8"import * as " + name + u8" from 'other';");
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_declaration"));  // (name)
    }
  }
}

TEST_F(test_parse_module, imported_modules_must_be_quoted) {
  for (string8 import_name : {u8"module", u8"not_a_keyword"}) {
    test_parser p(u8"import { test } from " + import_name + u8";",
                  capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors, ElementsAre(DIAG_TYPE_OFFSETS(
                              p.code, diag_cannot_import_from_unquoted_module,
                              import_name, strlen(u8"import { test } from "),
                              import_name)));
  }
}

TEST_F(test_parse_module,
       imported_variables_cannot_be_named_reserved_keywords) {
  for (string8 name : strict_reserved_keywords) {
    {
      string8 code = u8"import { " + name + u8" } from 'other';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_declaration"));  // (name)
      EXPECT_THAT(p.errors,
                  ElementsAre(DIAG_TYPE_OFFSETS(
                      p.code, diag_cannot_import_variable_named_keyword,  //
                      import_name, strlen(u8"import { "), name)));
    }

    {
      string8 code =
          u8"import { someFunction as " + name + u8" } from 'other';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_declaration"));  // (name)
      EXPECT_THAT(
          p.errors,
          ElementsAre(DIAG_TYPE_OFFSETS(
              p.code, diag_cannot_import_variable_named_keyword,  //
              import_name, strlen(u8"import { someFunction as "), name)));
    }

    {
      string8 code =
          u8"import { 'someFunction' as " + name + u8" } from 'other';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.variable_declarations, ElementsAre(import_decl(name)));
      EXPECT_THAT(
          p.errors,
          ElementsAre(DIAG_TYPE_OFFSETS(
              p.code, diag_cannot_import_variable_named_keyword,  //
              import_name, strlen(u8"import { 'someFunction' as "), name)));
    }

    {
      string8 code = u8"import " + name + u8" from 'other';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_declaration"));  // (name)
      EXPECT_THAT(p.errors,
                  ElementsAre(DIAG_TYPE_OFFSETS(
                      p.code, diag_cannot_import_variable_named_keyword,  //
                      import_name, strlen(u8"import "), name)));
    }

    {
      string8 code = u8"import * as " + name + u8" from 'other';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_declaration"));  // (name)
      EXPECT_THAT(p.errors,
                  ElementsAre(DIAG_TYPE_OFFSETS(
                      p.code, diag_cannot_import_variable_named_keyword,  //
                      import_name, strlen(u8"import * as "), name)));
    }
  }

  // TODO(strager): Test u8"await" and u8"yield".
  // TODO(#73): Disallow 'protected', 'implements', etc.
  for (string8 keyword : disallowed_binding_identifier_keywords) {
    string8 imported_variable = escape_first_character_in_keyword(keyword);

    {
      string8 code = u8"import { " + imported_variable + u8" } from 'other';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.variable_declarations, ElementsAre(import_decl(keyword)));
      EXPECT_THAT(p.errors,
                  ElementsAre(DIAG_TYPE_OFFSETS(
                      p.code, diag_keywords_cannot_contain_escape_sequences,  //
                      escape_sequence, strlen(u8"import { "), u8"\\u{??}")));
    }

    {
      string8 code = u8"import { someFunction as " + imported_variable +
                     u8" } from 'other';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.variable_declarations, ElementsAre(import_decl(keyword)));
      EXPECT_THAT(p.errors,
                  ElementsAre(DIAG_TYPE_OFFSETS(
                      p.code, diag_keywords_cannot_contain_escape_sequences,  //
                      escape_sequence, strlen(u8"import { someFunction as "),
                      u8"\\u{??}")));
    }

    {
      string8 code = u8"import { 'someFunction' as " + imported_variable +
                     u8" } from 'other';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.variable_declarations, ElementsAre(import_decl(keyword)));
      EXPECT_THAT(p.errors,
                  ElementsAre(DIAG_TYPE_OFFSETS(
                      p.code, diag_keywords_cannot_contain_escape_sequences,  //
                      escape_sequence, strlen(u8"import { 'someFunction' as "),
                      u8"\\u{??}")));
    }

    {
      string8 code = u8"import " + imported_variable + u8" from 'other';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.variable_declarations, ElementsAre(import_decl(keyword)));
      EXPECT_THAT(p.errors,
                  ElementsAre(DIAG_TYPE_OFFSETS(
                      p.code, diag_keywords_cannot_contain_escape_sequences,  //
                      escape_sequence, strlen(u8"import "), u8"\\u{??}")));
    }

    {
      string8 code = u8"import * as " + imported_variable + u8" from 'other';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code, capture_diags);
      p.parse_and_visit_statement();
      EXPECT_THAT(p.variable_declarations, ElementsAre(import_decl(keyword)));
      EXPECT_THAT(p.errors,
                  ElementsAre(DIAG_TYPE_OFFSETS(
                      p.code, diag_keywords_cannot_contain_escape_sequences,  //
                      escape_sequence, strlen(u8"import * as "), u8"\\u{??}")));
    }
  }
}

TEST_F(test_parse_module, exported_names_can_be_named_keywords) {
  for (string8 export_name : keywords) {
    {
      string8 code = u8"export {someFunction as " + export_name + u8"};";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code.c_str());
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_export_use"));  // someFunction
      EXPECT_THAT(p.variable_uses, ElementsAre(u8"someFunction"));
    }

    {
      string8 code = u8"export * as " + export_name + u8" from 'other-module';";
      SCOPED_TRACE(out_string8(code));
      test_parser p(code.c_str());
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits, IsEmpty());
    }
  }
}

TEST_F(test_parse_module, imported_names_can_be_named_keywords) {
  for (string8 import_name : keywords) {
    string8 code =
        u8"import {" + import_name + u8" as someFunction} from 'somewhere';";
    SCOPED_TRACE(out_string8(code));
    test_parser p(code.c_str());
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits,
                ElementsAre("visit_variable_declaration"));  // someFunction
    EXPECT_THAT(p.variable_declarations,
                ElementsAre(import_decl(u8"someFunction")));
  }
}

TEST_F(
    test_parse_module,
    imported_and_exported_names_can_be_reserved_keywords_with_escape_sequences) {
  for (string8 keyword : keywords) {
    string8 exported_name = escape_first_character_in_keyword(keyword);

    {
      padded_string code(u8"import {" + exported_name +
                         u8" as someFunction} from 'somewhere';");
      SCOPED_TRACE(code);
      test_parser p(code.string_view());
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_declaration"));  // someFunction
    }

    {
      padded_string code(u8"export {someFunction as " + exported_name + u8"};");
      SCOPED_TRACE(code);
      test_parser p(code.string_view());
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits,
                  ElementsAre("visit_variable_export_use"));  // someFunction
    }

    {
      padded_string code(u8"export * as " + exported_name + u8" from 'other';");
      SCOPED_TRACE(code);
      test_parser p(code.string_view());
      p.parse_and_visit_statement();
      EXPECT_THAT(p.visits, IsEmpty());
    }
  }
}

TEST_F(test_parse_module, import_requires_semicolon_or_newline) {
  {
    test_parser p(u8"import fs from 'fs' nextStatement"_sv, capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAre("visit_variable_declaration",  // fs
                                      "visit_variable_use",  // nextStatement
                                      "visit_end_of_module"));
    EXPECT_THAT(p.errors, ElementsAre(DIAG_TYPE_OFFSETS(
                              p.code,
                              diag_missing_semicolon_after_statement,  //
                              where, strlen(u8"import fs from 'fs'"), u8"")));
  }
}
}
}

// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew "strager" Glazar
//
// This file is part of quick-lint-js.
//
// quick-lint-js is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// quick-lint-js is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with quick-lint-js.  If not, see <https://www.gnu.org/licenses/>.
