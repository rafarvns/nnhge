// tr() (spec 029 FR-23, FR-33): the current language, else its chain, else the base; a key no table has shows its name
// (logged once) in Debug/Development and "??" in Release; key names on demand; run-time keys; resolve() for the UI.

#include <bit>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "crash/crash_internal.hpp"
#include "loc/loc_lookup.hpp"
#include "loc_fixture.hpp"
#include "nnh/build.hpp"
#include "nnh/localization.hpp"
#include "test.hpp"

using namespace loc_test;
using namespace font_test;
namespace fake = nnh::platform_fake;

namespace {

void started(const char* language, LocBuilder b = four_languages()) {
    fresh_engine();
    b.install();
    fake::set_system_language("en");
    CHECK(nnh::loc::start({.language = language}).ok());
}

std::string text(nnh::loc::Key k) { return nnh::loc::tr(k); }

} // namespace

NNH_TEST(loc_tr_reads_the_current_language) {
    const StopAtEnd stop_at_end;
    started("ja");
    CHECK(text("menu.new_game") == "ニューゲーム");
    started("pt-BR");
    CHECK(text("menu.new_game") == "Novo jogo");
    CHECK(text("item.sword") == "espada");
}

NNH_TEST(loc_tr_falls_back_along_the_chain_to_the_base) {
    const StopAtEnd stop_at_end;
    started("pt-BR");
    CHECK(text("menu.quit") == "Quit"); // pt-BR lacks it: the base's
    LocBuilder b = four_languages();
    b.add("pt-PT", "Português").key("menu.new_game", "Novo jogo (PT)");
    b.languages[4].fallback = 1; // pt-PT → pt-BR → en
    b.languages[1].keys.push_back({"only.br", "só no Brasil", {}, ""});
    started("pt-PT", b);
    CHECK(text("menu.new_game") == "Novo jogo (PT)");
    CHECK(text("only.br") == "só no Brasil");
    CHECK(text("menu.quit") == "Quit");
}

NNH_TEST(loc_tr_of_a_plural_key_is_its_other_form) {
    const StopAtEnd stop_at_end;
    started("en");
    CHECK(text("items.count") == "{count} items");
}

NNH_TEST(loc_tr_of_an_unknown_key_shows_its_name_once_logged) {
    const StopAtEnd stop_at_end;
    started("en");
    LogLines warnings(nnh::LogLevel::Warn);
    const std::string a = text("no.such.key");
    const std::string b = text("no.such.key");
    if (nnh::build::kDevToolsEnabled) {
        CHECK(a == "no.such.key");
        CHECK(warnings.count("unknown key \"no.such.key\"") == 1);
        CHECK(std::string(nnh::loc::resolve(0x1234ABCDu)) == "#1234ABCD");
    } else {
        CHECK(a == "??");
        CHECK(b == "??");
    }
}

NNH_TEST(loc_tr_never_returns_null_and_is_empty_before_start) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    nnh::detail::crash_install();
    const bool crashed = fake::run_expecting_crash([] { (void)nnh::loc::tr("menu.new_game"); });
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (!crashed) CHECK(std::string(nnh::loc::tr("menu.new_game")).empty());
}

NNH_TEST(loc_runtime_keys_match_literal_keys) {
    const StopAtEnd stop_at_end;
    started("pt-BR");
    const char* name = "menu.new_game";
    CHECK(nnh::loc::key(name).hash == nnh::loc::Key("menu.new_game").hash);
    CHECK(text(nnh::loc::key(name)) == "Novo jogo");
    CHECK(std::string(nnh::loc::resolve(nnh::loc::Key("item.sword").hash)) == "espada");
}

NNH_TEST(loc_show_keys_returns_the_names) {
    const StopAtEnd stop_at_end;
    started("pt-BR");
    nnh::loc::set_show_keys(true);
    if (nnh::build::kDevToolsEnabled) {
        CHECK(text("menu.new_game") == "menu.new_game");
        CHECK(text("menu.quit") == "menu.quit"); // the base's name
    } else {
        CHECK(text("menu.new_game") == "Novo jogo");
    }
    nnh::loc::set_show_keys(false);
    CHECK(text("menu.new_game") == "Novo jogo");
}

NNH_TEST(loc_tr_finds_every_key_with_many_buckets_and_none) {
    const StopAtEnd stop_at_end;
    LocBuilder b;
    TestLanguage& en = b.add("en", "English");
    char n[16];
    for (int i = 0; i < 300; ++i) {
        std::snprintf(n, sizeof n, "%d", i);
        en.key(joined("k.", n), joined("v", n));
    }
    started("en", b);
    for (int i = 0; i < 300; ++i) {
        std::snprintf(n, sizeof n, "%d", i);
        const std::string name = joined("k.", n);
        CHECK(text(nnh::loc::key(name.c_str())) == joined("v", n));
    }
}

NNH_TEST(loc_dump_describes_the_current_language) {
    const StopAtEnd stop_at_end;
    started("ja");
    LogLines info(nnh::LogLevel::Info);
    nnh::loc::dump();
    if (nnh::build::kDevToolsEnabled) CHECK(info.contains("loc: ja (日本語), 4 languages, chain of 2 tables"));
    else CHECK(info.lines.empty());
}

NNH_TEST(loc_show_keys_from_the_environment_at_start) {
    const StopAtEnd stop_at_end;
#if defined(_WIN32)
    _putenv("NNH_LOC_SHOW_KEYS=1");
#else
    setenv("NNH_LOC_SHOW_KEYS", "1", 1);
#endif
    started("pt-BR");
#if defined(_WIN32)
    _putenv("NNH_LOC_SHOW_KEYS=");
#else
    unsetenv("NNH_LOC_SHOW_KEYS");
#endif
    CHECK(text("menu.new_game") == (nnh::build::kDevToolsEnabled ? "menu.new_game" : "Novo jogo"));
    nnh::loc::stop();
    started("pt-BR"); // without the variable: texts again
    CHECK(text("menu.new_game") == "Novo jogo");
}

NNH_TEST(loc_lookup_forms_before_counts_like_popcount) {
    // T10.3: the table popcount, against std::popcount for every forms byte and category
    int wrong = 0;
    for (int forms = 0; forms < 64; ++forms) {
        for (int c = 0; c < nnh::detail::loc::kCategoryCount; ++c) {
            const int want = std::popcount(static_cast<unsigned>(forms & ((1 << c) - 1)));
            wrong += nnh::detail::loc::forms_before(static_cast<std::uint8_t>(forms), c) != want ? 1 : 0;
        }
    }
    CHECK(wrong == 0);
}
