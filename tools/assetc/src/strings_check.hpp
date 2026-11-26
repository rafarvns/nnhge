#pragma once
// Checking string files and building tables (spec 029 FR-17, FR-19, FR-20; plan "Build checks"): placeholders
// (`{name}`, `{{`/`}}`), a language against the base (unknown keys with "did you mean", placeholders, plural categories
// its rule needs, combining marks, length limits, missing translations), then the `.nnhstr` bytes. Errors stop the
// build; warnings ("file: warning: …") don't, unless strict. Tests: tests/test_strings_check.cpp.

#include <cstdint>
#include <string>
#include <vector>

#include "strings_model.hpp"

namespace nnh::assetc {

struct Placeholder {
    std::string name;
    int at = 0;   ///< byte offset of '{' in the stored text
    int span = 0; ///< bytes of "{name}"
};

/// Parses `text`'s placeholders and writes the stored text ("{{" → "{", "}}" → "}"). @return "" or the problem.
std::string parse_placeholders(const std::string& text, std::string& stored, std::vector<Placeholder>& out);

struct CheckResult {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/// The plural rule of `lang` (shipped or its own [language.plural]) compiled. @return "" or the problem.
std::string language_rule(const StringsLanguage& lang, std::vector<std::uint8_t>& bytecode);

/// Checks `lang` against `base` (the base is checked against itself).
void check_language(const StringsLanguage& lang, const StringsLanguage& base, CheckResult& result);

/// The `.nnhstr` bytes of `lang` (names kept unless `release`). Its own keys only (the chain does the rest).
std::vector<std::uint8_t> write_table(const StringsLanguage& lang, bool release);

} // namespace nnh::assetc
