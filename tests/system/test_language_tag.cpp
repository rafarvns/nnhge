// Language tags (spec 029 FR-15, FR-27): BCP 47-style codes, well-formed as plan 029 fixes them — 2–3 lowercase letters,
// an optional -Xxxx script, an optional -XX or -999 region, at most 15 bytes — and split into their parts.

#include <cstring>

#include "system/language_tag.hpp"
#include "test.hpp"

using nnh::detail::language_tag_well_formed;
using nnh::detail::LanguageTagParts;
using nnh::detail::split_language_tag;

NNH_TEST(language_tags_of_every_form_are_well_formed) {
    CHECK(language_tag_well_formed("en"));
    CHECK(language_tag_well_formed("ast"));
    CHECK(language_tag_well_formed("pt-BR"));
    CHECK(language_tag_well_formed("zh-Hant"));
    CHECK(language_tag_well_formed("es-419"));
    CHECK(language_tag_well_formed("zh-Hans-CN"));
    CHECK(language_tag_well_formed("sr-Latn-001"));
    CHECK(language_tag_well_formed("qps"));
}

NNH_TEST(language_tags_with_wrong_letters_or_parts_are_refused) {
    CHECK(!language_tag_well_formed(nullptr));
    CHECK(!language_tag_well_formed(""));
    CHECK(!language_tag_well_formed("e"));
    CHECK(!language_tag_well_formed("engl"));
    CHECK(!language_tag_well_formed("EN"));
    CHECK(!language_tag_well_formed("En"));
    CHECK(!language_tag_well_formed("pt-br"));
    CHECK(!language_tag_well_formed("pt-B"));
    CHECK(!language_tag_well_formed("pt-BRA"));
    CHECK(!language_tag_well_formed("es-41"));
    CHECK(!language_tag_well_formed("es-4190"));
    CHECK(!language_tag_well_formed("zh-hant"));
    CHECK(!language_tag_well_formed("zh-HANT"));
    CHECK(!language_tag_well_formed("zh-Hant-"));
    CHECK(!language_tag_well_formed("-en"));
    CHECK(!language_tag_well_formed("en-"));
    CHECK(!language_tag_well_formed("en_US"));
    CHECK(!language_tag_well_formed("en-US-x"));
    CHECK(!language_tag_well_formed("pt-BR-Latn")); // the script comes before the region
    CHECK(!language_tag_well_formed("en US"));
    CHECK(!language_tag_well_formed("\xC3\xA9n"));
}

NNH_TEST(a_language_tag_is_split_into_language_script_and_region) {
    LanguageTagParts p;
    CHECK(split_language_tag("zh-Hans-CN", p));
    CHECK(std::strcmp(p.language, "zh") == 0 && std::strcmp(p.script, "Hans") == 0 && std::strcmp(p.region, "CN") == 0);
    CHECK(split_language_tag("es-419", p));
    CHECK(std::strcmp(p.language, "es") == 0 && p.script[0] == 0 && std::strcmp(p.region, "419") == 0);
    CHECK(split_language_tag("ja", p));
    CHECK(std::strcmp(p.language, "ja") == 0 && p.script[0] == 0 && p.region[0] == 0);
    CHECK(!split_language_tag("ja-", p));
}

NNH_TEST(language_tags_are_compared_without_case) {
    using nnh::detail::same_language_tag;
    CHECK(same_language_tag("pt-BR", "pt-br"));
    CHECK(same_language_tag("ZH-hant", "zh-Hant"));
    CHECK(!same_language_tag("pt", "pt-BR"));
    CHECK(!same_language_tag("pt-BR", "pt-PT"));
}
