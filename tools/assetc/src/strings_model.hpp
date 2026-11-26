#pragma once
// String files (spec 029 FR-15..FR-18, D50; plan "String files"): what nnh-assetc reads of one language — its keys
// (dotted names, plural forms grouped) and its [language] metadata — with the file and line of everything, for messages.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "plural_compile.hpp"

namespace nnh::assetc {

struct Where {
    std::string file;
    int line = 0;
};

struct StringKey {
    std::string text;          ///< not a plural key: its text (escapes decoded, "{{" kept as written)
    std::string forms[6];      ///< a plural key: its forms by category (zero … other); "" = absent
    std::uint8_t form_bits = 0;
    std::string plural_arg;    ///< `<key>.plural = "name"`
    Where where;               ///< the first line that set it
    bool has_text = false;     ///< a text was set (it may be "")
    bool plural() const { return form_bits != 0; }
};

struct LanguageMeta {
    std::string name;          ///< the display name, in the language itself
    std::string fallback;      ///< a shipped code, or "" (then the base)
    std::string grouping = ",";
    int grouping_min = 4;
    std::vector<std::pair<std::string, std::string>> fonts; ///< (role, font asset path)
    std::vector<PluralSource> plural;                       ///< a rule for a language nnh-assetc doesn't know
    std::vector<std::pair<std::string, int>> limits;        ///< (key prefix, maximum characters), base only
    Where name_at, fallback_at, fonts_at;
};

struct StringsLanguage {
    std::string code;
    std::vector<std::string> files;
    std::map<std::string, StringKey> keys; ///< by dotted name
    LanguageMeta meta;
};

/// One line of a language file (`code<TAB>file;file…`) list (the build's): languages in the declared order, the base's
/// code flagged by `*` before it.
struct LanguageList {
    std::vector<std::string> codes;
    std::vector<std::vector<std::string>> files;
    int base = 0;
};

} // namespace nnh::assetc
