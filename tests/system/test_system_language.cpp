// nnh::system::language() (spec 029 FR-27): the platform's language, or a test script's override while one is set;
// never null.

#include <cstring>

#include "nnh/platform_fake.hpp"
#include "nnh/system.hpp"
#include "system/system_internal.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;

namespace {

bool is(const char* got, const char* want) { return got != nullptr && std::strcmp(got, want) == 0; }

} // namespace

NNH_TEST(system_language_is_english_on_the_fake_by_default) {
    fake::reset();
    CHECK(is(nnh::system::language(), "en"));
}

NNH_TEST(system_language_is_the_platforms_value) {
    fake::reset();
    fake::set_system_language("ja");
    CHECK(is(nnh::system::language(), "ja"));
    fake::set_system_language("pt-BR");
    CHECK(is(nnh::system::language(), "pt-BR"));
    fake::reset();
}

NNH_TEST(a_script_override_wins_until_it_is_released) {
    fake::reset();
    fake::set_system_language("de");
    nnh::detail::set_language_override("ru");
    CHECK(is(nnh::system::language(), "ru"));
    nnh::detail::set_language_override(nullptr);
    CHECK(is(nnh::system::language(), "de"));
    fake::reset();
}

NNH_TEST(an_override_is_copied_and_an_ill_formed_one_is_ignored) {
    fake::reset();
    char code[8] = "ko";
    nnh::detail::set_language_override(code);
    code[0] = 'x';
    CHECK(is(nnh::system::language(), "ko"));
    nnh::detail::set_language_override("not a tag");
    CHECK(is(nnh::system::language(), "ko"));
    nnh::detail::set_language_override(nullptr);
    CHECK(is(nnh::system::language(), "en"));
}

NNH_TEST(system_language_is_never_null_even_when_the_platform_has_none) {
    fake::reset();
    fake::set_system_language(nullptr);
    CHECK(is(nnh::system::language(), "en"));
    fake::set_system_language("bad tag");
    CHECK(is(nnh::system::language(), "en"));
    fake::reset();
}
