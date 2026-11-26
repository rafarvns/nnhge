#pragma once
// Character sets of font descriptions (spec 029 FR-3; plan "Font descriptions"): the named sets, "U+XXXX" and
// "U+XXXX..U+YYYY", and the characters of text (".chars" files, string files). Tests: tests/test_charsets.cpp.

#include <set>
#include <string>
#include <string_view>

namespace nnh::assetc {

/// Adds a named set or a "U+" code point or range to `out` ("language-names" adds nothing: the converter fills it from
/// the game's string files). @return "" or the problem.
std::string add_charset(std::string_view entry, std::set<char32_t>& out);

/// Adds every character of UTF-8 `text` but controls (< U+0020, U+007F..U+009F) to `out`.
void add_text_characters(std::string_view text, std::set<char32_t>& out);

} // namespace nnh::assetc
