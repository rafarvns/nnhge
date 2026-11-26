// Switching languages (spec 029 FR-25, FR-27, FR-28): set_language() takes effect at the start of the next frame (its
// pointers valid until then; the last call wins), publishes LanguageChanged delivered in that frame, refuses a code not
// shipped, keeps the old language when the new one can't load, and follows the system language when asked.

#include <string>

#include "crash/crash_internal.hpp"
#include "loc_fixture.hpp"
#include "nnh/build.hpp"
#include "nnh/events.hpp"
#include "nnh/localization.hpp"
#include "nnh/memory.hpp"
#include "test.hpp"

using namespace loc_test;
using namespace font_test;
namespace fake = nnh::platform_fake;

namespace {

int g_changes = 0;
nnh::LanguageChanged g_last;
std::string g_seen_in_handler;

void started(const char* language, bool follow = false) {
    fresh_engine();
    four_languages().install();
    fake::set_system_language("en");
    CHECK(nnh::loc::start({.language = language, .follow_system = follow}).ok());
    CHECK(nnh::frame()); // start's LanguageChanged delivered
}

nnh::Subscription watch() {
    g_changes = 0;
    g_seen_in_handler.clear();
    return nnh::events::subscribe<nnh::LanguageChanged>([](const nnh::LanguageChanged& e) {
        ++g_changes;
        g_last = e;
        g_seen_in_handler = nnh::loc::tr("menu.new_game");
    });
}

} // namespace

NNH_TEST(loc_switch_takes_effect_at_the_next_frame) {
    const StopAtEnd stop_at_end;
    started("en");
    auto sub = watch();
    const char* before = nnh::loc::tr("menu.new_game");
    CHECK(nnh::loc::set_language("ja").ok());
    CHECK(std::string(before) == "New game"); // still valid
    CHECK(std::string(nnh::loc::code()) == "en");
    CHECK(nnh::frame());
    CHECK(std::string(nnh::loc::code()) == "ja");
    CHECK(std::string(nnh::loc::tr("menu.new_game")) == "ニューゲーム");
    CHECK(g_changes == 1);
    CHECK(g_last.previous == 0);
    CHECK(g_last.current == 3);
    CHECK(g_last.serial == 2);
    CHECK(nnh::loc::serial() == 2);
    CHECK(g_seen_in_handler == "ニューゲーム"); // handlers see the new language
}

NNH_TEST(loc_switch_the_last_call_of_a_frame_wins) {
    const StopAtEnd stop_at_end;
    started("en");
    auto sub = watch();
    CHECK(nnh::loc::set_language("ja").ok());
    CHECK(nnh::loc::set_language("ru").ok());
    CHECK(nnh::frame());
    CHECK(std::string(nnh::loc::code()) == "ru");
    CHECK(g_changes == 1);
}

NNH_TEST(loc_switch_to_the_current_language_publishes_nothing) {
    const StopAtEnd stop_at_end;
    started("en");
    auto sub = watch();
    CHECK(nnh::loc::set_language("EN").ok());
    CHECK(nnh::frame());
    CHECK(g_changes == 0);
    CHECK(nnh::loc::serial() == 1);
}

NNH_TEST(loc_switch_refuses_a_code_not_shipped) {
    const StopAtEnd stop_at_end;
    started("en");
    auto sub = watch();
    auto r = nnh::loc::set_language("fr");
    CHECK(!r.ok());
    if (!r) CHECK(r.error().code == nnh::ErrorCode::NotFound);
    CHECK(nnh::frame());
    CHECK(g_changes == 0);
    CHECK(std::string(nnh::loc::code()) == "en");
}

NNH_TEST(loc_switch_keeps_the_language_when_the_new_one_fails) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    const LocBuilder b = four_languages();
    for (const TestLanguage& l : b.languages) {
        std::vector<std::uint8_t>& t = keep(write_table(l));
        if (l.code == "ja") t[90] ^= 1; // damaged
        add_table_asset(l, t);
    }
    test_assets().push_back({".nnh_loc", &keep(b.manifest())});
    remount();
    fake::set_system_language("en");
    CHECK(nnh::loc::start({.language = "pt-BR"}).ok());
    CHECK(nnh::frame());
    auto sub = watch();
    LogLines errors(nnh::LogLevel::Error);
    CHECK(nnh::loc::set_language("ja").ok());
    CHECK(nnh::frame());
    CHECK(g_changes == 0);
    CHECK(std::string(nnh::loc::code()) == "pt-BR");
    CHECK(std::string(nnh::loc::tr("menu.new_game")) == "Novo jogo");
    CHECK(std::string(nnh::loc::tr("menu.quit")) == "Quit");
    CHECK(errors.contains("language ja: bad CRC (staying in pt-BR)"));
}

NNH_TEST(loc_switch_follows_the_system_language) {
    const StopAtEnd stop_at_end;
    started(nullptr, true);
    auto sub = watch();
    fake::set_system_language("ru-RU");
    CHECK(nnh::frame());
    CHECK(std::string(nnh::loc::code()) == "ru");
    CHECK(g_changes == 1);
    fake::set_system_language("fr"); // not shipped: the base
    CHECK(nnh::frame());
    CHECK(std::string(nnh::loc::code()) == "en");
    CHECK(nnh::loc::set_language("ja").ok()); // the player's choice turns following off
    CHECK(nnh::frame());
    fake::set_system_language("pt-BR");
    CHECK(nnh::frame());
    CHECK(std::string(nnh::loc::code()) == "ja");
    nnh::loc::follow_system(true);
    CHECK(nnh::frame());
    CHECK(std::string(nnh::loc::code()) == "pt-BR");
}

NNH_TEST(loc_switch_following_the_system_never_picks_the_pseudo_language) {
    // review N3: the same guard as start()'s: the pseudo-language only when chosen
    const StopAtEnd stop_at_end;
    fresh_engine();
    LocBuilder b = four_languages();
    b.add("qps", "Pseudo").key("menu.new_game", "[Ñéŵ ĝåɱé~~]");
    b.pseudo = 4;
    b.install();
    fake::set_system_language("ja");
    CHECK(nnh::loc::start({.follow_system = true}).ok());
    CHECK(nnh::frame());
    CHECK(std::string(nnh::loc::code()) == "ja");
    fake::set_system_language("qps");
    CHECK(nnh::frame());
    CHECK(std::string(nnh::loc::code()) == "en");
}

NNH_TEST(loc_switch_does_not_follow_unless_asked) {
    const StopAtEnd stop_at_end;
    started("en");
    fake::set_system_language("ja");
    CHECK(nnh::frame());
    CHECK(std::string(nnh::loc::code()) == "en");
}

NNH_TEST(loc_switch_allocates_nothing) {
    const StopAtEnd stop_at_end;
    started("en");
    const int live = nnh::mem::live_allocations();
    const char* order[] = {"ja", "ru", "pt-BR", "en"};
    for (int i = 0; i < 40; ++i) {
        CHECK(nnh::loc::set_language(order[i % 4]).ok());
        CHECK(nnh::frame());
    }
    CHECK(nnh::mem::live_allocations() == live);
    CHECK(nnh::loc::serial() == 41);
}

NNH_TEST(loc_set_language_before_start_asserts) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    nnh::detail::crash_install();
    const bool crashed = fake::run_expecting_crash([] { (void)nnh::loc::set_language("en"); });
    CHECK(crashed == nnh::build::kAssertsEnabled);
}
