// PC platform only (spec 029 FR-27): the simulator's language.ini — `language=<code>` lines, read bounded; comments,
// other keys, ill-formed codes and overlong lines are ignored. Linked into nnh_conformance_pc.

#include <cstring>

#include "language_pc.hpp"
#include "test.hpp"

namespace pc = nnh::platform::pc;

namespace {

bool reads(const char* text, const char* want) {
    char out[16] = "?";
    const bool found = pc::language_from_ini(text, std::strlen(text), out);
    return want == nullptr ? !found : (found && std::strcmp(out, want) == 0);
}

} // namespace

NNH_TEST(pc_language_ini_gives_the_language_line) {
    CHECK(reads("language=ja\n", "ja"));
    CHECK(reads("language=pt-BR", "pt-BR"));
    CHECK(reads("  language = zh-Hant  \r\n", "zh-Hant"));
    CHECK(reads("# simulator\nscale=2\nlanguage=de\n", "de"));
}

NNH_TEST(pc_language_ini_ignores_comments_other_keys_and_ill_formed_codes) {
    CHECK(reads("", nullptr));
    CHECK(reads("# language=ja\n", nullptr));
    CHECK(reads("lang=ja\n", nullptr));
    CHECK(reads("language=Japanese\n", nullptr));
    CHECK(reads("language=\n", nullptr));
    CHECK(reads("language=pt-br\nlanguage=ru\n", "ru")); // the ill-formed line skipped, the next one read
}

NNH_TEST(pc_language_ini_stops_at_its_bounds) {
    char big[600];
    std::memset(big, 'x', sizeof big);
    std::memcpy(big + 550, "\nlanguage=ko\n", 13);
    big[sizeof big - 1] = 0;
    CHECK(reads(big, "ko")); // an overlong first line is skipped whole
    CHECK(!pc::language_from_ini("language=ja", 10, big)); // only `size` bytes are read: "j" isn't a code
}
