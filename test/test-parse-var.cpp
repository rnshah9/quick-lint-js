// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew Glazar
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quick-lint-js/array.h>
#include <quick-lint-js/char8.h>
#include <quick-lint-js/cli-location.h>
#include <quick-lint-js/error-collector.h>
#include <quick-lint-js/error-matcher.h>
#include <quick-lint-js/error.h>
#include <quick-lint-js/language.h>
#include <quick-lint-js/padded-string.h>
#include <quick-lint-js/parse-support.h>
#include <quick-lint-js/parse.h>
#include <quick-lint-js/spy-visitor.h>
#include <string>
#include <string_view>
#include <vector>

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace quick_lint_js {
namespace {
TEST(test_parse, parse_simple_let) {
  {
    spy_visitor v = parse_and_visit_statement(u8"let x"_sv);
    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"x");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_let);
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let a, b"_sv);
    ASSERT_EQ(v.variable_declarations.size(), 2);
    EXPECT_EQ(v.variable_declarations[0].name, u8"a");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_let);
    EXPECT_EQ(v.variable_declarations[1].name, u8"b");
    EXPECT_EQ(v.variable_declarations[1].kind, variable_kind::_let);
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let a, b, c, d, e, f, g"_sv);
    ASSERT_EQ(v.variable_declarations.size(), 7);
    EXPECT_EQ(v.variable_declarations[0].name, u8"a");
    EXPECT_EQ(v.variable_declarations[1].name, u8"b");
    EXPECT_EQ(v.variable_declarations[2].name, u8"c");
    EXPECT_EQ(v.variable_declarations[3].name, u8"d");
    EXPECT_EQ(v.variable_declarations[4].name, u8"e");
    EXPECT_EQ(v.variable_declarations[5].name, u8"f");
    EXPECT_EQ(v.variable_declarations[6].name, u8"g");
    for (const auto &declaration : v.variable_declarations) {
      EXPECT_EQ(declaration.kind, variable_kind::_let);
    }
  }

  {
    spy_visitor v;
    padded_string code(u8"let first; let second"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"first");
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    ASSERT_EQ(v.variable_declarations.size(), 2);
    EXPECT_EQ(v.variable_declarations[0].name, u8"first");
    EXPECT_EQ(v.variable_declarations[1].name, u8"second");
    EXPECT_THAT(v.errors, IsEmpty());
  }
}

TEST(test_parse, parse_simple_var) {
  spy_visitor v;
  padded_string code(u8"var x"_sv);
  parser p(&code, &v);
  EXPECT_TRUE(p.parse_and_visit_statement(v));
  ASSERT_EQ(v.variable_declarations.size(), 1);
  EXPECT_EQ(v.variable_declarations[0].name, u8"x");
  EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_var);
  EXPECT_THAT(v.errors, IsEmpty());
}

TEST(test_parse, parse_simple_const) {
  spy_visitor v;
  padded_string code(u8"const x"_sv);
  parser p(&code, &v);
  EXPECT_TRUE(p.parse_and_visit_statement(v));
  ASSERT_EQ(v.variable_declarations.size(), 1);
  EXPECT_EQ(v.variable_declarations[0].name, u8"x");
  EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_const);
  EXPECT_THAT(v.errors, IsEmpty());
}

TEST(test_parse, parse_let_with_initializers) {
  {
    spy_visitor v = parse_and_visit_statement(u8"let x = 2"_sv);
    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"x");
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let x = 2, y = 3"_sv);
    ASSERT_EQ(v.variable_declarations.size(), 2);
    EXPECT_EQ(v.variable_declarations[0].name, u8"x");
    EXPECT_EQ(v.variable_declarations[1].name, u8"y");
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let x = other, y = x"_sv);
    ASSERT_EQ(v.variable_declarations.size(), 2);
    EXPECT_EQ(v.variable_declarations[0].name, u8"x");
    EXPECT_EQ(v.variable_declarations[1].name, u8"y");
    ASSERT_EQ(v.variable_uses.size(), 2);
    EXPECT_EQ(v.variable_uses[0].name, u8"other");
    EXPECT_EQ(v.variable_uses[1].name, u8"x");
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let x = y in z;"_sv);
    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"x");
    ASSERT_EQ(v.variable_uses.size(), 2);
    EXPECT_EQ(v.variable_uses[0].name, u8"y");
    EXPECT_EQ(v.variable_uses[1].name, u8"z");
  }
}

TEST(test_parse, parse_let_with_object_destructuring) {
  {
    spy_visitor v = parse_and_visit_statement(u8"let {x} = 2"_sv);
    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"x");
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let {x, y, z} = 2"_sv);
    ASSERT_EQ(v.variable_declarations.size(), 3);
    EXPECT_EQ(v.variable_declarations[0].name, u8"x");
    EXPECT_EQ(v.variable_declarations[1].name, u8"y");
    EXPECT_EQ(v.variable_declarations[2].name, u8"z");
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let {key: variable} = 2"_sv);
    EXPECT_THAT(v.visits, ElementsAre("visit_variable_declaration"));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"variable", variable_kind::_let}));
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let {} = x;"_sv);
    EXPECT_THAT(v.variable_declarations, IsEmpty());
    ASSERT_EQ(v.variable_uses.size(), 1);
  }

  {
    spy_visitor v =
        parse_and_visit_statement(u8"let {key = defaultValue} = x;"_sv);
    EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",  // x
                                      "visit_variable_use",  // defaultValue
                                      "visit_variable_declaration"));  // key
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"key", variable_kind::_let}));
    EXPECT_THAT(
        v.variable_uses,
        ElementsAre(spy_visitor::visited_variable_use{u8"x"},  //
                    spy_visitor::visited_variable_use{u8"defaultValue"}));
  }
}

TEST(test_parse, parse_let_with_array_destructuring) {
  {
    spy_visitor v = parse_and_visit_statement(u8"let [first, second] = xs;"_sv);
    EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",            // x
                                      "visit_variable_declaration",    // first
                                      "visit_variable_declaration"));  // second
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(
                    spy_visitor::visited_variable_declaration{
                        u8"first", variable_kind::_let},
                    spy_visitor::visited_variable_declaration{
                        u8"second", variable_kind::_let}));
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"xs"}));
  }
}

TEST(test_parse,
     variables_used_in_let_initializer_are_used_before_variable_declaration) {
  using namespace std::literals::string_view_literals;

  spy_visitor v;
  padded_string code(u8"let x = x"_sv);
  parser p(&code, &v);
  EXPECT_TRUE(p.parse_and_visit_statement(v));

  EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",  //
                                    "visit_variable_declaration"));

  ASSERT_EQ(v.variable_declarations.size(), 1);
  EXPECT_EQ(v.variable_declarations[0].name, u8"x");
  ASSERT_EQ(v.variable_uses.size(), 1);
  EXPECT_EQ(v.variable_uses[0].name, u8"x");
  EXPECT_THAT(v.errors, IsEmpty());
}

TEST(test_parse, parse_invalid_let) {
  {
    spy_visitor v;
    padded_string code(u8"let"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations, IsEmpty());
    EXPECT_THAT(v.errors, ElementsAre(ERROR_TYPE_FIELD(
                              error_let_with_no_bindings, where,
                              offsets_matcher(&code, 0, u8"let"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"let a,"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_EQ(v.variable_declarations.size(), 1);
    EXPECT_THAT(v.errors,
                ElementsAre(ERROR_TYPE_FIELD(
                    error_stray_comma_in_let_statement, where,
                    offsets_matcher(&code, strlen(u8"let a"), u8","))));
  }

  {
    spy_visitor v;
    padded_string code(u8"let x, 42"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_EQ(v.variable_declarations.size(), 1);
    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_unexpected_token_in_variable_declaration, unexpected_token,
            offsets_matcher(&code, strlen(u8"let x, "), u8"42"))));
  }

  for (string8 keyword : disallowed_binding_identifier_keywords) {
    {
      padded_string code(u8"var " + keyword);
      SCOPED_TRACE(code);
      spy_visitor v;
      parser p(&code, &v);
      EXPECT_TRUE(p.parse_and_visit_statement(v));
      EXPECT_THAT(v.variable_declarations, IsEmpty());
      EXPECT_THAT(v.errors,
                  ElementsAre(ERROR_TYPE_FIELD(
                      error_cannot_declare_variable_with_keyword_name, keyword,
                      offsets_matcher(&code, strlen(u8"var "), keyword))));
    }

    {
      padded_string code(u8"var " + keyword + u8";");
      SCOPED_TRACE(code);
      spy_visitor v;
      parser p(&code, &v);
      EXPECT_TRUE(p.parse_and_visit_statement(v));
      EXPECT_THAT(v.variable_declarations, IsEmpty());
      EXPECT_THAT(v.errors,
                  ElementsAre(ERROR_TYPE_FIELD(
                      error_cannot_declare_variable_with_keyword_name, keyword,
                      offsets_matcher(&code, strlen(u8"var "), keyword))));
    }

    {
      padded_string code(u8"var " + keyword + u8" = x;");
      SCOPED_TRACE(code);
      spy_visitor v;
      parser p(&code, &v);
      EXPECT_TRUE(p.parse_and_visit_statement(v));
      EXPECT_THAT(v.variable_declarations, IsEmpty());
      EXPECT_THAT(v.visits, ElementsAre("visit_variable_use"));  // x
      EXPECT_THAT(v.errors,
                  ElementsAre(ERROR_TYPE_FIELD(
                      error_cannot_declare_variable_with_keyword_name, keyword,
                      offsets_matcher(&code, strlen(u8"var "), keyword))));
    }
  }

  {
    padded_string code(u8"let while (x) { break; }"_sv);
    spy_visitor v;
    parser p(&code, &v);
    p.parse_and_visit_module(v);
    EXPECT_THAT(v.variable_declarations, IsEmpty());
    EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",       // x
                                      "visit_enter_block_scope",  //
                                      "visit_exit_block_scope",   //
                                      "visit_end_of_module"));
    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_unexpected_token_in_variable_declaration, unexpected_token,
            offsets_matcher(&code, strlen(u8"let "), u8"while"))));
  }

  {
    padded_string code(u8"let\nwhile (x) { break; }"_sv);
    spy_visitor v;
    parser p(&code, &v);
    p.parse_and_visit_module(v);
    EXPECT_THAT(v.variable_declarations, IsEmpty());
    EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",       // x
                                      "visit_enter_block_scope",  //
                                      "visit_exit_block_scope",   //
                                      "visit_end_of_module"));
    EXPECT_THAT(v.errors, ElementsAre(ERROR_TYPE_FIELD(
                              error_let_with_no_bindings, where,
                              offsets_matcher(&code, 0, u8"let"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"let 42*69"_sv);
    parser p(&code, &v);
    p.parse_and_visit_module(v);
    EXPECT_EQ(v.variable_declarations.size(), 0);
    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_unexpected_token_in_variable_declaration, unexpected_token,
            offsets_matcher(&code, strlen(u8"let "), u8"42"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"let {debugger}"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_EQ(v.variable_declarations.size(), 0);
    EXPECT_THAT(
        v.errors,
        UnorderedElementsAre(
            ERROR_TYPE_FIELD(
                error_missing_value_for_object_literal_entry, key,
                offsets_matcher(&code, strlen(u8"let {"), u8"debugger")),
            ERROR_TYPE_FIELD(
                error_invalid_binding_in_let_statement, where,
                offsets_matcher(&code, strlen(u8"let {"), u8"debugger"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"let {42}"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_EQ(v.variable_declarations.size(), 0);
    EXPECT_THAT(v.errors,
                UnorderedElementsAre(
                    ERROR_TYPE_FIELD(
                        error_invalid_lone_literal_in_object_literal, where,
                        offsets_matcher(&code, strlen(u8"let {"), u8"42")),
                    ERROR_TYPE_FIELD(
                        error_invalid_binding_in_let_statement, where,
                        offsets_matcher(&code, strlen(u8"let {"), u8"42"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"let true, true, y\nlet x;"_sv);
    parser p(&code, &v);
    p.parse_and_visit_module(v);
    EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",          // y
                                      "visit_variable_declaration",  // x
                                      "visit_end_of_module"));
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"y"}));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"x", variable_kind::_let}));
    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_unexpected_token_in_variable_declaration, unexpected_token,
            offsets_matcher(&code, strlen(u8"let "), u8"true"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"const = y, z = w, = x;"_sv);
    parser p(&code, &v);
    p.parse_and_visit_module(v);
    EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",          // y
                                      "visit_variable_use",          // w
                                      "visit_variable_declaration",  // z
                                      "visit_variable_use",          // x
                                      "visit_end_of_module"));
    EXPECT_THAT(v.errors,
                UnorderedElementsAre(
                    ERROR_TYPE_FIELD(
                        error_missing_variable_name_in_declaration, equal_token,
                        offsets_matcher(&code, strlen(u8"const "), u8"=")),
                    ERROR_TYPE_FIELD(
                        error_missing_variable_name_in_declaration, equal_token,
                        offsets_matcher(&code, strlen(u8"const = y, z = w, "),
                                        u8"="))));
  }
}

TEST(test_parse, report_missing_semicolon_for_declarations) {
  {
    spy_visitor v;
    padded_string code(u8"let x = 2 for (;;) { console.log(); }"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"x", variable_kind::_let}));
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"console"}));
    cli_source_position::offset_type end_of_let_statement =
        strlen(u8"let x = 2");
    EXPECT_THAT(v.errors,
                ElementsAre(ERROR_TYPE_FIELD(
                    error_missing_semicolon_after_statement, where,
                    offsets_matcher(&code, end_of_let_statement, u8""))));
  }
  {
    spy_visitor v;
    padded_string code(u8"const x debugger"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"x", variable_kind::_const}));
    cli_source_position::offset_type end_of_const_statement =
        strlen(u8"const x");
    EXPECT_THAT(v.errors,
                ElementsAre(ERROR_TYPE_FIELD(
                    error_missing_semicolon_after_statement, where,
                    offsets_matcher(&code, end_of_const_statement, u8""))));
  }
}

TEST(test_parse, old_style_variables_can_be_named_let) {
  {
    spy_visitor v = parse_and_visit_statement(u8"var let = initial;");
    EXPECT_THAT(v.visits,
                ElementsAre("visit_variable_use",            // initial
                            "visit_variable_declaration"));  // let
    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"let");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_var);
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"function let(let) {}");
    EXPECT_THAT(v.visits,
                ElementsAre("visit_variable_declaration",  // let (function)
                            "visit_enter_function_scope",
                            "visit_variable_declaration",  // let (parameter)
                            "visit_enter_function_scope_body",
                            "visit_exit_function_scope"));
    ASSERT_EQ(v.variable_declarations.size(), 2);
    EXPECT_EQ(v.variable_declarations[0].name, u8"let");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_function);
    EXPECT_EQ(v.variable_declarations[1].name, u8"let");
    EXPECT_EQ(v.variable_declarations[1].kind, variable_kind::_parameter);
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"(function let() {})");
    EXPECT_THAT(
        v.visits,
        ElementsAre("visit_enter_named_function_scope",  // let (function)
                    "visit_enter_function_scope_body",
                    "visit_exit_function_scope"));
    EXPECT_THAT(
        v.enter_named_function_scopes,
        ElementsAre(spy_visitor::visited_enter_named_function_scope{u8"let"}));
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"try { } catch (let) { }");
    EXPECT_THAT(v.visits, ElementsAre("visit_enter_block_scope",     //
                                      "visit_exit_block_scope",      //
                                      "visit_enter_block_scope",     //
                                      "visit_variable_declaration",  // let
                                      "visit_exit_block_scope"));
    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"let");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_catch);
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let {x = let} = o;");
    EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",            // o
                                      "visit_variable_use",            // let
                                      "visit_variable_declaration"));  // x
    ASSERT_EQ(v.variable_uses.size(), 2);
    EXPECT_EQ(v.variable_uses[1].name, u8"let");
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"console.log(let);");
    EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",    // console
                                      "visit_variable_use"));  // let
    ASSERT_EQ(v.variable_uses.size(), 2);
    EXPECT_EQ(v.variable_uses[1].name, u8"let");
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let.method();");
    EXPECT_THAT(v.visits,
                ElementsAre("visit_variable_use"));  // let
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"let"}));
  }

  for (string8 code : {
           u8"(async let => null)",
           u8"(async (let) => null)",
           u8"(let => null)",
           u8"((let) => null)",
       }) {
    SCOPED_TRACE(out_string8(code));
    spy_visitor v = parse_and_visit_statement(code);
    EXPECT_THAT(v.visits, ElementsAre("visit_enter_function_scope",       //
                                      "visit_variable_declaration",       // let
                                      "visit_enter_function_scope_body",  //
                                      "visit_exit_function_scope"));
    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"let");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_parameter);
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"for (let in xs) ;");
    EXPECT_THAT(v.visits,
                // TODO(strager): A for scope shouldn't be introduced by
                // this syntax. (No variable is being declared.)
                ElementsAre("visit_enter_for_scope",      //
                            "visit_variable_use",         // xs
                            "visit_variable_assignment",  // let
                            "visit_exit_for_scope"));
    EXPECT_THAT(v.variable_assignments,
                ElementsAre(spy_visitor::visited_variable_assignment{u8"let"}));
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"for (let.prop in xs) ;");
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"xs"},  //
                            spy_visitor::visited_variable_use{u8"let"}));
  }
}

TEST(test_parse, new_style_variables_cannot_be_named_let) {
  for (string8 declaration_kind : {u8"const", u8"let"}) {
    spy_visitor v;
    padded_string code(declaration_kind + u8" let = null;");
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));

    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_cannot_declare_variable_named_let_with_let, name,
            offsets_matcher(&code, declaration_kind.size() + 1, u8"let"))));

    EXPECT_THAT(v.visits, ElementsAre("visit_variable_declaration"));
    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"let");
  }

  {
    spy_visitor v;
    padded_string code(u8"let {other, let} = stuff;"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_cannot_declare_variable_named_let_with_let, name,
            offsets_matcher(&code, strlen(u8"let {other, "), u8"let"))));
  }

  // import implies strict mode (because modules imply strict mode).
  {
    spy_visitor v;
    padded_string code(u8"import let from 'weird';"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.errors,
                ElementsAre(ERROR_TYPE_FIELD(
                    error_cannot_import_let, import_name,
                    offsets_matcher(&code, strlen(u8"import "), u8"let"))));

    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"let");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_import);
  }

  // import implies strict mode (because modules imply strict mode).
  {
    spy_visitor v;
    padded_string code(u8"import * as let from 'weird';"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_cannot_import_let, import_name,
            offsets_matcher(&code, strlen(u8"import * as "), u8"let"))));

    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"let");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_import);
  }

  // import implies strict mode (because modules imply strict mode).
  {
    spy_visitor v;
    padded_string code(u8"import { let } from 'weird';"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.errors,
                ElementsAre(ERROR_TYPE_FIELD(
                    error_cannot_import_let, import_name,
                    offsets_matcher(&code, strlen(u8"import { "), u8"let"))));

    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"let");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_import);
  }

  // TODO(strager): export implies strict mode (because modules imply strict
  // mode).
  if ((false)) {
    spy_visitor v;
    padded_string code(u8"export function let() {}"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_cannot_export_let, export_name,
            offsets_matcher(&code, strlen(u8"export function "), u8"let"))));

    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"let");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_function);
  }

  // class implies strict mode.
  {
    spy_visitor v;
    padded_string code(u8"class let {}"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.errors,
                ElementsAre(ERROR_TYPE_FIELD(
                    error_cannot_declare_class_named_let, name,
                    offsets_matcher(&code, strlen(u8"class "), u8"let"))));

    ASSERT_EQ(v.variable_declarations.size(), 1);
    EXPECT_EQ(v.variable_declarations[0].name, u8"let");
    EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_class);
  }
}

TEST(test_parse, use_await_in_non_async_function) {
  {
    spy_visitor v = parse_and_visit_statement(u8"await(x);"_sv);
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"await"},  //
                            spy_visitor::visited_variable_use{u8"x"}));
  }

  {
    spy_visitor v = parse_and_visit_statement(
        u8"async function f() {\n"
        u8"  function g() { await(x); }\n"
        u8"}");
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"await"},  //
                            spy_visitor::visited_variable_use{u8"x"}));
  }

  {
    spy_visitor v = parse_and_visit_statement(
        u8"function f() {\n"
        u8"  async function g() {}\n"
        u8"  await();\n"
        u8"}");
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"await"}));
  }

  {
    spy_visitor v = parse_and_visit_statement(
        u8"(() => {\n"
        u8"  async () => {};\n"
        u8"  await();\n"
        u8"})");
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"await"}));
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"(async => { await(); })"_sv);
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"await"}));
  }

  {
    spy_visitor v =
        parse_and_visit_statement(u8"({ async() { await(); } })"_sv);
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"await"}));
  }

  {
    spy_visitor v =
        parse_and_visit_statement(u8"class C { async() { await(); } }"_sv);
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"await"}));
  }
}

TEST(test_parse, declare_await_in_non_async_function) {
  {
    spy_visitor v = parse_and_visit_statement(u8"function await() { }"_sv);
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"await", variable_kind::_function}));
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let await = 42;"_sv);
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"await", variable_kind::_let}));
  }

  {
    spy_visitor v = parse_and_visit_statement(
        u8"(async function() {\n"
        u8"  (function(await) { })\n"
        u8"})");
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"await", variable_kind::_parameter}));
  }

  {
    spy_visitor v = parse_and_visit_statement(
        u8"(function() {\n"
        u8"  async function await() { }\n"
        u8"})");
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"await", variable_kind::_function}));
  }
}

TEST(test_parse, declare_await_in_async_function) {
  {
    spy_visitor v;
    padded_string code(u8"function await() { }"_sv);
    parser p(&code, &v);
    auto guard = p.enter_function(function_attributes::async);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"await", variable_kind::_function}));
    // TODO(strager): Include a note referencing the origin of the async
    // function.
    EXPECT_THAT(v.errors,
                ElementsAre(ERROR_TYPE_FIELD(
                    error_cannot_declare_await_in_async_function, name,
                    offsets_matcher(&code, strlen(u8"function "), u8"await"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"var await;"_sv);
    parser p(&code, &v);
    auto guard = p.enter_function(function_attributes::async);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"await", variable_kind::_var}));
    EXPECT_THAT(v.errors,
                ElementsAre(ERROR_TYPE_FIELD(
                    error_cannot_declare_await_in_async_function, name,
                    offsets_matcher(&code, strlen(u8"var "), u8"await"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"try {} catch (await) {}"_sv);
    parser p(&code, &v);
    auto guard = p.enter_function(function_attributes::async);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"await", variable_kind::_catch}));
    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_cannot_declare_await_in_async_function, name,
            offsets_matcher(&code, strlen(u8"try {} catch ("), u8"await"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"async function f(await) {}"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(
                    spy_visitor::visited_variable_declaration{
                        u8"f", variable_kind::_function},  //
                    spy_visitor::visited_variable_declaration{
                        u8"await", variable_kind::_parameter}));
    EXPECT_THAT(v.errors,
                UnorderedElementsAre(
                    ERROR_TYPE_FIELD(
                        error_cannot_declare_await_in_async_function, name,
                        offsets_matcher(&code, strlen(u8"async function f("),
                                        u8"await")),
                    // TODO(strager): Drop the
                    // error_missing_operand_for_operator error.
                    ERROR_TYPE_FIELD(error_missing_operand_for_operator, where,
                                     testing::_)));
  }
}

TEST(
    test_parse,
    declare_await_in_async_function_is_allowed_for_named_function_expressions) {
  {
    spy_visitor v = parse_and_visit_statement(
        u8"(async function() {\n"
        u8"  (function await() { await; })(); \n"
        u8"})();");
    EXPECT_THAT(v.visits,
                ElementsAre("visit_enter_function_scope",        //
                            "visit_enter_function_scope_body",   //
                            "visit_enter_named_function_scope",  // await
                            "visit_enter_function_scope_body",   //
                            "visit_variable_use",                // await
                            "visit_exit_function_scope",         //
                            "visit_exit_function_scope"));
    EXPECT_THAT(v.enter_named_function_scopes,
                ElementsAre(spy_visitor::visited_enter_named_function_scope{
                    u8"await"}));
  }
}

TEST(test_parse, use_yield_in_non_generator_function) {
  {
    spy_visitor v = parse_and_visit_statement(u8"yield(x);"_sv);
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"yield"},  //
                            spy_visitor::visited_variable_use{u8"x"}));
  }

  {
    spy_visitor v = parse_and_visit_statement(
        u8"function* f() {\n"
        u8"  function g() { yield(x); }\n"
        u8"}");
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"yield"},  //
                            spy_visitor::visited_variable_use{u8"x"}));
  }

  {
    spy_visitor v = parse_and_visit_statement(
        u8"function f() {\n"
        u8"  function* g() {}\n"
        u8"  yield();\n"
        u8"}");
    EXPECT_THAT(v.variable_uses,
                ElementsAre(spy_visitor::visited_variable_use{u8"yield"}));
  }
}

TEST(test_parse, declare_yield_in_non_generator_function) {
  {
    spy_visitor v = parse_and_visit_statement(u8"function yield() { }"_sv);
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"yield", variable_kind::_function}));
  }

  {
    spy_visitor v = parse_and_visit_statement(u8"let yield = 42;"_sv);
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"yield", variable_kind::_let}));
  }

  {
    spy_visitor v = parse_and_visit_statement(
        u8"(async function() {\n"
        u8"  (function(yield) { })\n"
        u8"})");
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"yield", variable_kind::_parameter}));
  }

  {
    spy_visitor v = parse_and_visit_statement(
        u8"(function() {\n"
        u8"  function* yield() { }\n"
        u8"})");
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"yield", variable_kind::_function}));
  }
}

TEST(test_parse, declare_yield_in_generator_function) {
  {
    spy_visitor v;
    padded_string code(u8"function yield() { }"_sv);
    parser p(&code, &v);
    auto guard = p.enter_function(function_attributes::generator);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"yield", variable_kind::_function}));
    // TODO(strager): Include a note referencing the origin of the generator
    // function.
    EXPECT_THAT(v.errors,
                ElementsAre(ERROR_TYPE_FIELD(
                    error_cannot_declare_yield_in_generator_function, name,
                    offsets_matcher(&code, strlen(u8"function "), u8"yield"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"var yield;"_sv);
    parser p(&code, &v);
    auto guard = p.enter_function(function_attributes::generator);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"yield", variable_kind::_var}));
    EXPECT_THAT(v.errors,
                ElementsAre(ERROR_TYPE_FIELD(
                    error_cannot_declare_yield_in_generator_function, name,
                    offsets_matcher(&code, strlen(u8"var "), u8"yield"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"try {} catch (yield) {}"_sv);
    parser p(&code, &v);
    auto guard = p.enter_function(function_attributes::generator);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(spy_visitor::visited_variable_declaration{
                    u8"yield", variable_kind::_catch}));
    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_cannot_declare_yield_in_generator_function, name,
            offsets_matcher(&code, strlen(u8"try {} catch ("), u8"yield"))));
  }

  {
    spy_visitor v;
    padded_string code(u8"function* f(yield) {}"_sv);
    parser p(&code, &v);
    EXPECT_TRUE(p.parse_and_visit_statement(v));
    EXPECT_THAT(v.variable_declarations,
                ElementsAre(
                    spy_visitor::visited_variable_declaration{
                        u8"f", variable_kind::_function},  //
                    spy_visitor::visited_variable_declaration{
                        u8"yield", variable_kind::_parameter}));
    EXPECT_THAT(
        v.errors,
        ElementsAre(ERROR_TYPE_FIELD(
            error_cannot_declare_yield_in_generator_function, name,
            offsets_matcher(&code, strlen(u8"function* f("), u8"yield"))));
  }
}

TEST(test_parse, variables_can_be_named_contextual_keywords) {
  for (string8 name :
       {u8"as", u8"async", u8"await", u8"from", u8"get", u8"of", u8"private",
        u8"protected", u8"public", u8"set", u8"static", u8"yield"}) {
    SCOPED_TRACE(out_string8(name));

    {
      spy_visitor v =
          parse_and_visit_statement(u8"var " + name + u8" = initial;");
      EXPECT_THAT(v.visits,
                  ElementsAre("visit_variable_use",            // initial
                              "visit_variable_declaration"));  // (name)
      ASSERT_EQ(v.variable_declarations.size(), 1);
      EXPECT_EQ(v.variable_declarations[0].name, name);
      EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_var);
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"let " + name + u8" = initial;");
      EXPECT_THAT(v.visits,
                  ElementsAre("visit_variable_use",            // initial
                              "visit_variable_declaration"));  // (name)
      ASSERT_EQ(v.variable_declarations.size(), 1);
      EXPECT_EQ(v.variable_declarations[0].name, name);
      EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_let);
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"const " + name + u8" = initial;");
      EXPECT_THAT(v.visits,
                  ElementsAre("visit_variable_use",            // initial
                              "visit_variable_declaration"));  // (name)
      ASSERT_EQ(v.variable_declarations.size(), 1);
      EXPECT_EQ(v.variable_declarations[0].name, name);
      EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_const);
    }

    {
      spy_visitor v = parse_and_visit_statement(u8"function " + name + u8"(" +
                                                name + u8") {}");
      EXPECT_THAT(
          v.visits,
          ElementsAre("visit_variable_declaration",       // (name) (function)
                      "visit_enter_function_scope",       //
                      "visit_variable_declaration",       // (name) (parameter)
                      "visit_enter_function_scope_body",  //
                      "visit_exit_function_scope"));
      ASSERT_EQ(v.variable_declarations.size(), 2);
      EXPECT_EQ(v.variable_declarations[0].name, name);
      EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_function);
      EXPECT_EQ(v.variable_declarations[1].name, name);
      EXPECT_EQ(v.variable_declarations[1].kind, variable_kind::_parameter);
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"(function " + name + u8"() {})");
      EXPECT_THAT(
          v.visits,
          ElementsAre("visit_enter_named_function_scope",  // (name) (function)
                      "visit_enter_function_scope_body",   //
                      "visit_exit_function_scope"));
      EXPECT_THAT(
          v.enter_named_function_scopes,
          ElementsAre(spy_visitor::visited_enter_named_function_scope{name}));
    }

    {
      spy_visitor v = parse_and_visit_statement(u8"class " + name + u8" {}");
      EXPECT_THAT(v.visits,
                  ElementsAre("visit_variable_declaration",  // (name)
                              "visit_enter_class_scope",     //
                              "visit_exit_class_scope"));
      EXPECT_THAT(v.variable_declarations,
                  ElementsAre(spy_visitor::visited_variable_declaration{
                      name, variable_kind::_class}));
    }

    {
      spy_visitor v = parse_and_visit_statement(u8"(class " + name + u8" {})");
      EXPECT_THAT(v.visits,
                  ElementsAre("visit_enter_class_scope",     //
                              "visit_variable_declaration",  // (name)
                              "visit_exit_class_scope"));
      EXPECT_THAT(v.variable_declarations,
                  ElementsAre(spy_visitor::visited_variable_declaration{
                      name, variable_kind::_class}));
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"try { } catch (" + name + u8") { }");
      EXPECT_THAT(v.visits, ElementsAre("visit_enter_block_scope",     //
                                        "visit_exit_block_scope",      //
                                        "visit_enter_block_scope",     //
                                        "visit_variable_declaration",  // (name)
                                        "visit_exit_block_scope"));
      ASSERT_EQ(v.variable_declarations.size(), 1);
      EXPECT_EQ(v.variable_declarations[0].name, name);
      EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_catch);
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"let {x = " + name + u8"} = o;");
      EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",  // o
                                        "visit_variable_use",  // (name)
                                        "visit_variable_declaration"));  // x
      ASSERT_EQ(v.variable_uses.size(), 2);
      EXPECT_EQ(v.variable_uses[1].name, name);
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"console.log(" + name + u8");");
      EXPECT_THAT(v.visits, ElementsAre("visit_variable_use",    // console
                                        "visit_variable_use"));  // (name)
      ASSERT_EQ(v.variable_uses.size(), 2);
      EXPECT_EQ(v.variable_uses[1].name, name);
    }

    {
      string8 code = name + u8";";
      SCOPED_TRACE(out_string8(code));
      spy_visitor v = parse_and_visit_statement(code.c_str());
      EXPECT_THAT(v.visits, ElementsAre("visit_variable_use"));  // (name)
      ASSERT_EQ(v.variable_uses.size(), 1);
      EXPECT_EQ(v.variable_uses[0].name, name);
    }

    {
      spy_visitor v = parse_and_visit_statement(name + u8".method();");
      EXPECT_THAT(v.visits,
                  ElementsAre("visit_variable_use"));  // (name)
      EXPECT_THAT(v.variable_uses,
                  ElementsAre(spy_visitor::visited_variable_use{name}));
    }

    for (string8 code : {
             u8"(async " + name + u8" => null)",
             u8"(async (" + name + u8") => null)",
             u8"(" + name + u8" => null)",
             u8"((" + name + u8") => null)",
         }) {
      SCOPED_TRACE(out_string8(code));
      spy_visitor v = parse_and_visit_statement(code);
      EXPECT_THAT(v.visits, ElementsAre("visit_enter_function_scope",  //
                                        "visit_variable_declaration",  // (name)
                                        "visit_enter_function_scope_body",  //
                                        "visit_exit_function_scope"));
      ASSERT_EQ(v.variable_declarations.size(), 1);
      EXPECT_EQ(v.variable_declarations[0].name, name);
      EXPECT_EQ(v.variable_declarations[0].kind, variable_kind::_parameter);
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"for (" + name + u8" in xs) ;");
      EXPECT_THAT(v.visits,
                  ElementsAre("visit_variable_use",           // xs
                              "visit_variable_assignment"));  // (name)
      EXPECT_THAT(v.variable_assignments,
                  ElementsAre(spy_visitor::visited_variable_assignment{name}));
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"for (" + name + u8".prop in xs) ;");
      EXPECT_THAT(v.variable_uses,
                  ElementsAre(spy_visitor::visited_variable_use{name},  //
                              spy_visitor::visited_variable_use{u8"xs"}));
    }

    if (name != u8"async") {
      // NOTE(strager): async isn't allowed here. See
      // test_parse.cannot_assign_to_variable_named_async_in_for_of.
      spy_visitor v =
          parse_and_visit_statement(u8"for (" + name + u8" of xs) ;");
      EXPECT_THAT(v.variable_assignments,
                  ElementsAre(spy_visitor::visited_variable_assignment{name}));
      EXPECT_THAT(v.variable_uses,
                  ElementsAre(spy_visitor::visited_variable_use{u8"xs"}));
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"for ((" + name + u8") of xs) ;");
      EXPECT_THAT(v.variable_assignments,
                  ElementsAre(spy_visitor::visited_variable_assignment{name}));
      EXPECT_THAT(v.variable_uses,
                  ElementsAre(spy_visitor::visited_variable_use{u8"xs"}));
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"for (" + name + u8".prop of xs) ;");
      EXPECT_THAT(v.variable_assignments, IsEmpty());
      EXPECT_THAT(v.variable_uses,
                  ElementsAre(spy_visitor::visited_variable_use{name},
                              spy_visitor::visited_variable_use{u8"xs"}));
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"for (" + name + u8"; cond;) ;");
      EXPECT_THAT(v.variable_assignments, IsEmpty());
      EXPECT_THAT(v.variable_uses,
                  ElementsAre(spy_visitor::visited_variable_use{name},
                              spy_visitor::visited_variable_use{u8"cond"}));
    }

    {
      spy_visitor v =
          parse_and_visit_statement(u8"for (" + name + u8".prop; cond;) ;");
      EXPECT_THAT(v.variable_assignments, IsEmpty());
      EXPECT_THAT(v.variable_uses,
                  ElementsAre(spy_visitor::visited_variable_use{name},
                              spy_visitor::visited_variable_use{u8"cond"}));
    }
  }
}
}
}
