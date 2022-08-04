// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

#include <algorithm>
#include <cstddef>
#include <memory>
#include <quick-lint-js/assert.h>
#include <quick-lint-js/c-api-diag-reporter.h>
#include <quick-lint-js/c-api.h>
#include <quick-lint-js/configuration/configuration.h>
#include <quick-lint-js/container/padded-string.h>
#include <quick-lint-js/document.h>
#include <quick-lint-js/fe/diagnostic-types.h>
#include <quick-lint-js/fe/lint.h>
#include <quick-lint-js/fe/parse.h>
#include <quick-lint-js/lsp/lsp-location.h>
#include <quick-lint-js/port/char8.h>
#include <quick-lint-js/web-demo-location.h>
#include <vector>

using namespace quick_lint_js;

namespace quick_lint_js {
namespace {
template <class Locator, class ErrorReporter>
class qljs_document_base {
 public:
  explicit qljs_document_base() { this->parser_options_.jsx = true; }

  const auto* lint() {
    this->diag_reporter_.reset();
    this->diag_reporter_.set_input(this->document_.string(),
                                   &this->document_.locator());
    parser p(this->document_.string(), &this->diag_reporter_,
             this->parser_options_);
    linter l(&this->diag_reporter_, &this->config_.globals());
    p.parse_and_visit_module_catching_fatal_parse_errors(l);

    return this->diag_reporter_.get_diagnostics();
  }

  const auto* lint_as_config_file() {
    this->diag_reporter_.reset();
    this->diag_reporter_.set_input(this->document_.string(),
                                   &this->document_.locator());
    configuration().load_from_json(this->document_.string(),
                                   &this->diag_reporter_);
    return this->diag_reporter_.get_diagnostics();
  }

  quick_lint_js::document<Locator> document_;
  ErrorReporter diag_reporter_;
  configuration config_;
  parser_options parser_options_;
};
}
}

struct qljs_web_demo_document final
    : public qljs_document_base<
          web_demo_locator,
          c_api_diag_reporter<qljs_web_demo_diagnostic, web_demo_locator>> {
 public:
  void set_text(string8_view replacement) {
    this->document_.set_text(replacement);
  }

  void set_config_text(string8_view text) {
    padded_string padded_text(text);
    this->config_.reset();
    this->config_.load_from_json(&padded_text, &null_diag_reporter::instance);
  }

  void set_translator(translator t) { this->diag_reporter_.set_translator(t); }
};

qljs_web_demo_document* qljs_web_demo_create_document(void) {
  qljs_web_demo_document* p = new qljs_web_demo_document();
  return p;
}

void qljs_web_demo_destroy_document(qljs_web_demo_document* p) { delete p; }

void qljs_web_demo_set_text(qljs_web_demo_document* p, const void* text_utf_8,
                            size_t text_byte_count) {
  p->set_text(string8_view(reinterpret_cast<const char8*>(text_utf_8),
                           text_byte_count));
}

void qljs_web_demo_set_config_text(qljs_web_demo_document* p,
                                   const void* text_utf_8,
                                   size_t text_byte_count) {
  p->set_config_text(string8_view(reinterpret_cast<const char8*>(text_utf_8),
                                  text_byte_count));
}

void qljs_web_demo_set_language_options(qljs_web_demo_document* p,
                                        qljs_language_options options) {
  p->parser_options_.jsx = options & qljs_language_options_jsx_bit;
  p->parser_options_.typescript =
      options & qljs_language_options_typescript_bit;
}

void qljs_web_demo_set_locale(qljs_web_demo_document* p, const char* locale) {
  translator t;
  t.use_messages_from_locale(locale);
  p->set_translator(t);
}

const qljs_web_demo_diagnostic* qljs_web_demo_lint(qljs_web_demo_document* p) {
  return p->lint();
}

const qljs_web_demo_diagnostic* qljs_web_demo_lint_as_config_file(
    qljs_web_demo_document* p) {
  return p->lint_as_config_file();
}

const char* const* qljs_list_locales() {
  static const char* const* locale_list = [] {
    std::size_t locale_count = translation_table_locale_count + 1;
    const char** locales = new const char*[locale_count + 1];

    std::size_t i = 0;
    const char* l;
    for (l = translation_data.locale_table; *l != '\0';
         l += std::strlen(l) + 1, ++i) {
      locales[i] = l;
    }
    locales[i] = l;  // Default locale (empty string).
    ++i;
    QLJS_ASSERT(i == locale_count);
    locales[i] = nullptr;  // Terminator.

    return locales;
  }();
  return locale_list;
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
