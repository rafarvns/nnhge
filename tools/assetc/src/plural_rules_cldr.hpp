#pragma once
// The 36 languages whose CLDR 46 integer cardinal plural rules nnh-assetc ships (spec 029 FR-17; plan "Shipped rules"),
// as CLDR rule text per category ("other" is implicit). Tests: tests/loc/test_plural_rules.cpp.

namespace nnh::assetc {

struct CldrCategoryRule {
    const char* category = nullptr; ///< "zero", "one", "two", "few", "many"
    const char* rule = nullptr;     ///< CLDR rule text (integer operands n, i; v w f t c e are 0)
};

struct CldrRule {
    const char* code;
    CldrCategoryRule categories[3];
};

/// The rule of `code` (its language's for a regional code, e.g. "pt-BR" → "pt"; "pt-PT" has its own), or nullptr.
const CldrRule* cldr_rule(const char* code);
int cldr_rule_count();

} // namespace nnh::assetc
