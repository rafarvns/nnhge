// loc::format (spec 029 FR-24, FR-17): placeholders by name (integers grouped as the language says, texts, other keys),
// the plural form the language's rule chooses, "{{" kept literal, truncation at a UTF-8 boundary, a missing argument.

#include <cstring>
#include <string>

#include "crash/crash_internal.hpp"
#include "loc_fixture.hpp"
#include "nnh/build.hpp"
#include "nnh/localization.hpp"
#include "test.hpp"

using namespace loc_test;
using namespace font_test;
namespace fake = nnh::platform_fake;

namespace {

void started(const char* language) {
    fresh_engine();
    four_languages().install();
    fake::set_system_language("en");
    CHECK(nnh::loc::start({.language = language}).ok());
}

template <typename... A>
std::string fmt(nnh::loc::Key key, const A&... args) {
    char out[128];
    const int n = nnh::loc::format(out, sizeof out, key, args...);
    CHECK(n == static_cast<int>(std::string(out).size()));
    return out;
}

} // namespace

NNH_TEST(loc_format_groups_integers_as_the_language_writes_them) {
    const StopAtEnd stop_at_end;
    started("en");
    CHECK(fmt("hud.score", nnh::loc::arg("score", 1234567)) == "Score: 1,234,567");
    CHECK(fmt("hud.score", nnh::loc::arg("score", -42)) == "Score: -42");
    started("pt-BR");
    CHECK(fmt("hud.score", nnh::loc::arg("score", 1234567)) == "Pontos: 1.234.567");
    started("ru"); // ru lacks hud.score: the base's text, ru's digits
    CHECK(fmt("hud.score", nnh::loc::arg("score", 1234)) == "Score: 1234");
    CHECK(fmt("hud.score", nnh::loc::arg("score", 12345)) == "Score: 12\xC2\xA0" "345");
}

NNH_TEST(loc_format_chooses_plural_forms) {
    const StopAtEnd stop_at_end;
    started("en");
    CHECK(fmt("items.count", nnh::loc::arg("count", 1)) == "1 item");
    CHECK(fmt("items.count", nnh::loc::arg("count", 0)) == "0 items");
    CHECK(fmt("items.count", nnh::loc::arg("count", 2000)) == "2,000 items");
    CHECK(fmt("items.count", nnh::loc::arg("count", -1)) == "-1 item");
    started("ru");
    CHECK(fmt("items.count", nnh::loc::arg("count", 1)) == "1 предмет");
    CHECK(fmt("items.count", nnh::loc::arg("count", 21)) == "21 предмет");
    CHECK(fmt("items.count", nnh::loc::arg("count", 3)) == "3 предмета");
    CHECK(fmt("items.count", nnh::loc::arg("count", 11)) == "11 предметов");
    CHECK(fmt("items.count", nnh::loc::arg("count", 25)) == "25 предметов");
    started("ja"); // no rule: always "other"; a form the key lacks: "other"
    CHECK(fmt("items.count", nnh::loc::arg("count", 1)) == "アイテム1個");
}

NNH_TEST(loc_format_puts_texts_and_other_keys) {
    const StopAtEnd stop_at_end;
    started("en");
    CHECK(fmt("hud.hello", nnh::loc::arg("name", "Ana")) == "Hello, Ana!");
    CHECK(fmt("hud.found", nnh::loc::arg_key("item", "item.sword")) == "You found a sword.");
    started("pt-BR");
    CHECK(fmt("hud.found", nnh::loc::arg_key("item", "item.sword")) == "Você achou: espada.");
    CHECK(fmt("hud.hello", nnh::loc::arg("name", "Ana")) == "Hello, Ana!"); // the base's text
}

NNH_TEST(loc_format_keeps_escaped_braces_literal) {
    const StopAtEnd stop_at_end;
    started("en");
    CHECK(fmt("hud.braces", nnh::loc::arg("n", 5)) == "{literal} 5");
}

NNH_TEST(loc_format_without_an_argument_shows_the_placeholder) {
    const StopAtEnd stop_at_end;
    started("en");
    LogLines warnings(nnh::LogLevel::Warn);
    const std::string s = fmt("hud.hello", nnh::loc::arg("other", 1));
    if (nnh::build::kDevToolsEnabled) {
        CHECK(s == "Hello, {name}!");
        CHECK(warnings.contains("no argument for {name}"));
    } else {
        CHECK(s == "Hello, !");
    }
    CHECK(fmt("menu.new_game") == "New game");
}

NNH_TEST(loc_format_truncates_at_a_utf8_boundary) {
    const StopAtEnd stop_at_end;
    started("ja");
    char out[8]; // "ニューゲーム" is 6 × 3 bytes: 7 fit, the third character's last byte doesn't
    CHECK(nnh::loc::format(out, sizeof out, "menu.new_game") == 6);
    CHECK(std::string(out) == "ニュ");
    char tiny[1];
    CHECK(nnh::loc::format(tiny, 1, "menu.new_game") == 0);
    CHECK(tiny[0] == 0);
    started("en");
    char five[5];
    CHECK(nnh::loc::format(five, 5, "hud.score", nnh::loc::arg("score", 123456)) == 4);
    CHECK(std::string(five) == "Scor");
}

NNH_TEST(loc_format_of_an_unknown_key_writes_its_marker) {
    const StopAtEnd stop_at_end;
    started("en");
    const std::string s = fmt("no.key.here", nnh::loc::arg("n", 1));
    CHECK(s == (nnh::build::kDevToolsEnabled ? "no.key.here" : "??"));
}

NNH_TEST(loc_format_refuses_a_missing_buffer) {
    const StopAtEnd stop_at_end;
    started("en");
    nnh::detail::crash_install();
    const bool crashed = fake::run_expecting_crash([] { (void)nnh::loc::format(nullptr, 4, "menu.quit"); });
    CHECK(crashed == nnh::build::kAssertsEnabled);
}

NNH_TEST(loc_format_uses_the_rule_of_the_language_whose_text_it_is) {
    // ru is current but "items.en_only" exists only in en: 21 is "other" under en's rule (under ru's it would be "one")
    const StopAtEnd stop_at_end;
    fresh_engine();
    LocBuilder b = four_languages();
    b.languages[0].plural("items.en_only", "count", "{count} thing", "{count} things");
    b.install();
    fake::set_system_language("en");
    CHECK(nnh::loc::start({.language = "ru"}).ok());
    CHECK(fmt("items.en_only", nnh::loc::arg("count", 21)) == "21 things");
    CHECK(fmt("items.en_only", nnh::loc::arg("count", 1)) == "1 thing");
}

NNH_TEST(loc_format_a_form_the_key_lacks_is_its_other_form) {
    // a table the build would refuse (ru needs few and many), read safely: the missing forms fall back to "other"
    const StopAtEnd stop_at_end;
    fresh_engine();
    LocBuilder b = four_languages();
    b.languages[2].plural("items.short", "count", "{count} one", "{count} other");
    b.languages[2].plural("items.gap", "count", "{count} one", "{count} other", "", "{count} many");
    b.languages[0].plural("items.gap", "count", "{count} en-one", "{count} en-other");
    b.languages[0].plural("items.short", "count", "{count} en-one", "{count} en-other");
    b.install();
    fake::set_system_language("en");
    CHECK(nnh::loc::start({.language = "ru"}).ok());
    CHECK(fmt("items.short", nnh::loc::arg("count", 3)) == "3 other");  // few: absent
    CHECK(fmt("items.short", nnh::loc::arg("count", 5)) == "5 other");  // many: absent
    CHECK(fmt("items.short", nnh::loc::arg("count", 21)) == "21 one");
    CHECK(fmt("items.gap", nnh::loc::arg("count", 3)) == "3 other"); // few absent: "other", not the next form (many)
    CHECK(fmt("items.gap", nnh::loc::arg("count", 5)) == "5 many");
}

NNH_TEST(loc_format_clamps_unsigned_arguments_above_the_long_long_range) {
    // review N8: an unsigned long long above LLONG_MAX wrapped to a negative number
    CHECK(nnh::loc::arg("n", 18446744073709551615ull).number == 9223372036854775807ll);
    CHECK(nnh::loc::arg("n", 9223372036854775808ull).number == 9223372036854775807ll);
    CHECK(nnh::loc::arg("n", 9223372036854775807ull).number == 9223372036854775807ll);
    CHECK(nnh::loc::arg("n", 4294967295u).number == 4294967295ll);
    CHECK(nnh::loc::arg("n", -5).number == -5);
}

NNH_TEST(loc_format_never_writes_past_its_capacity) {
    // T10.3: spans are copied whole; a span one byte longer than the room left is cut too (8 bytes into 8: 7 + NUL)
    const StopAtEnd stop_at_end;
    started("en");
    struct {
        char out[8];
        char canary[4];
    } b;
    std::memset(&b, 'X', sizeof b);
    CHECK(nnh::loc::format(b.out, sizeof b.out, "menu.new_game") == 7); // "New game"
    CHECK(std::string(b.out) == "New gam");
    CHECK(b.canary[0] == 'X' && b.canary[1] == 'X');
    std::memset(&b, 'X', sizeof b);
    CHECK(nnh::loc::format(b.out, 7, "menu.new_game") == 6);
    CHECK(b.out[6] == 0 && b.out[7] == 'X');
}
