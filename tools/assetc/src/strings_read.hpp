#pragma once
// Reading string files (spec 029 FR-16, FR-18; plan "String files"): spec 014's TOML subset with the strings additions —
// table headers as key prefixes, dotted keys, basic strings and multi-line basic strings (a line ending with '\' joins
// the next, its leading spaces dropped), 1 MiB files, 4 KiB lines; the reserved [language] tables hold metadata. Every
// file of a language merges into one StringsLanguage; a key set twice (in any of its files) is an error. Problems are
// "file:line: reason". Tests: tests/test_strings_read.cpp.

#include <string>
#include <string_view>
#include <vector>

#include "strings_model.hpp"

namespace nnh::assetc {

inline constexpr std::size_t kMaxStringsFileBytes = 1u << 20;
inline constexpr std::size_t kMaxStringsLine = 4096;
inline constexpr std::size_t kMaxStringBytes = 4096;

/// Adds the keys and metadata of `text` (file `file`) to `into`. @return false when a problem was added.
bool read_strings(std::string_view text, const std::string& file, StringsLanguage& into, std::vector<std::string>& problems);

/// Reads the build's language list (strings/<code>.strings or strings/<code>/*.strings per language).
bool read_language_list(std::string_view text, LanguageList& out);

} // namespace nnh::assetc
