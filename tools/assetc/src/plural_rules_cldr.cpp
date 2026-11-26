// The plural rules nnh-assetc knows (plural_rules_cldr.hpp): CLDR 46's cardinal rules (common/supplemental/plurals.xml,
// Unicode CLDR, Unicode License v3; https://cldr.unicode.org) as rule text, the categories without their samples.
// Operands v, w, f, t, c and e are 0 for the integers NNHGE formats, so the compiler folds them.

#include "plural_rules_cldr.hpp"

#include <cstring>

namespace nnh::assetc {
namespace {

constexpr const char* kMillion = "e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5";

const CldrRule kRules[] = {
    {"ja", {}}, {"zh", {}}, {"ko", {}}, {"vi", {}}, {"id", {}}, {"ms", {}}, {"th", {}},
    {"en", {{"one", "i = 1 and v = 0"}}},
    {"de", {{"one", "i = 1 and v = 0"}}},
    {"nl", {{"one", "i = 1 and v = 0"}}},
    {"sv", {{"one", "i = 1 and v = 0"}}},
    {"da", {{"one", "n = 1 or t != 0 and i = 0,1"}}},
    {"nb", {{"one", "n = 1"}}},
    {"fi", {{"one", "i = 1 and v = 0"}}},
    {"et", {{"one", "i = 1 and v = 0"}}},
    {"it", {{"one", "i = 1 and v = 0"}, {"many", kMillion}}},
    {"el", {{"one", "n = 1"}}},
    {"hu", {{"one", "n = 1"}}},
    {"tr", {{"one", "n = 1"}}},
    {"bg", {{"one", "n = 1"}}},
    {"ca", {{"one", "i = 1 and v = 0"}, {"many", kMillion}}},
    {"es", {{"one", "n = 1"}, {"many", kMillion}}},
    {"fr", {{"one", "i = 0,1"}, {"many", kMillion}}},
    {"pt", {{"one", "i = 0..1"}, {"many", kMillion}}},
    {"pt-PT", {{"one", "i = 1 and v = 0"}, {"many", kMillion}}},
    {"ro", {{"one", "i = 1 and v = 0"}, {"few", "v != 0 or n = 0 or n != 1 and n % 100 = 1..19"}}},
    {"ru", {{"one", "v = 0 and i % 10 = 1 and i % 100 != 11"},
            {"few", "v = 0 and i % 10 = 2..4 and i % 100 != 12..14"},
            {"many", "v = 0 and i % 10 = 0 or v = 0 and i % 10 = 5..9 or v = 0 and i % 100 = 11..14"}}},
    {"uk", {{"one", "v = 0 and i % 10 = 1 and i % 100 != 11"},
            {"few", "v = 0 and i % 10 = 2..4 and i % 100 != 12..14"},
            {"many", "v = 0 and i % 10 = 0 or v = 0 and i % 10 = 5..9 or v = 0 and i % 100 = 11..14"}}},
    {"pl", {{"one", "i = 1 and v = 0"},
            {"few", "v = 0 and i % 10 = 2..4 and i % 100 != 12..14"},
            {"many", "v = 0 and i != 1 and i % 10 = 0..1 or v = 0 and i % 10 = 5..9 or v = 0 and i % 100 = 12..14"}}},
    {"cs", {{"one", "i = 1 and v = 0"}, {"few", "i = 2..4 and v = 0"}, {"many", "v != 0"}}},
    {"sk", {{"one", "i = 1 and v = 0"}, {"few", "i = 2..4 and v = 0"}, {"many", "v != 0"}}},
    {"hr", {{"one", "v = 0 and i % 10 = 1 and i % 100 != 11 or f % 10 = 1 and f % 100 != 11"},
            {"few", "v = 0 and i % 10 = 2..4 and i % 100 != 12..14 or f % 10 = 2..4 and f % 100 != 12..14"}}},
    {"sr", {{"one", "v = 0 and i % 10 = 1 and i % 100 != 11 or f % 10 = 1 and f % 100 != 11"},
            {"few", "v = 0 and i % 10 = 2..4 and i % 100 != 12..14 or f % 10 = 2..4 and f % 100 != 12..14"}}},
    {"sl", {{"one", "v = 0 and i % 100 = 1"}, {"two", "v = 0 and i % 100 = 2"}, {"few", "v = 0 and i % 100 = 3..4 or v != 0"}}},
    {"lt", {{"one", "n % 10 = 1 and n % 100 != 11..19"}, {"few", "n % 10 = 2..9 and n % 100 != 11..19"}, {"many", "f != 0"}}},
    {"lv", {{"zero", "n % 10 = 0 or n % 100 = 11..19 or v = 2 and f % 100 = 11..19"},
            {"one", "n % 10 = 1 and n % 100 != 11 or v = 2 and f % 10 = 1 and f % 100 != 11 or v != 2 and f % 10 = 1"}}},
};

} // namespace

const CldrRule* cldr_rule(const char* code) {
    for (const CldrRule& r : kRules)
        if (std::strcmp(r.code, code) == 0) return &r;
    // a regional code falls back to its language ("pt-BR" → "pt", "en-GB" → "en"), never across scripts
    const char* dash = std::strchr(code, '-');
    if (dash == nullptr) return nullptr;
    char language[8] = {};
    const std::size_t n = static_cast<std::size_t>(dash - code);
    if (n >= sizeof language) return nullptr;
    std::memcpy(language, code, n);
    const bool script = dash[1] >= 'A' && dash[1] <= 'Z' && dash[2] >= 'a' && dash[2] <= 'z';
    if (script && std::strcmp(language, "zh") != 0) return nullptr;
    for (const CldrRule& r : kRules)
        if (std::strcmp(r.code, language) == 0) return &r;
    return nullptr;
}

int cldr_rule_count() { return static_cast<int>(sizeof kRules / sizeof kRules[0]); }

} // namespace nnh::assetc
