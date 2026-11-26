// Starting localization (spec 029 FR-22, FR-26, FR-28): the starting language is the player's saved choice, else the
// system language matched (exact, then the same language with another region), else the base; LanguageChanged once;
// a missing or damaged manifest is an error that leaves nothing behind.

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

void installed(const char* system) {
    fresh_engine();
    four_languages().install();
    fake::set_system_language(system);
}

} // namespace

NNH_TEST(loc_start_without_a_manifest_is_not_found) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    auto r = nnh::loc::start();
    CHECK(!r.ok());
    if (!r) CHECK(r.error().code == nnh::ErrorCode::NotFound);
    CHECK(std::string(nnh::loc::code()).empty());
    CHECK(nnh::loc::language_count() == 0);
}

NNH_TEST(loc_start_picks_the_players_choice_first) {
    const StopAtEnd stop_at_end;
    installed("ja");
    CHECK(nnh::loc::start({.language = "PT-br"}).ok());
    CHECK(std::string(nnh::loc::code()) == "pt-BR");
    CHECK(std::string(nnh::loc::name()) == "Português (Brasil)");
    CHECK(nnh::loc::index() == 1);
    CHECK(nnh::loc::serial() == 1);
}

NNH_TEST(loc_start_matches_the_system_language) {
    const StopAtEnd stop_at_end;
    installed("ru");
    CHECK(nnh::loc::start({.language = "xx"}).ok()); // not shipped: the system's
    CHECK(std::string(nnh::loc::code()) == "ru");
    installed("pt-PT");
    CHECK(nnh::loc::start().ok()); // the same language, another region
    CHECK(std::string(nnh::loc::code()) == "pt-BR");
    installed("en-GB");
    CHECK(nnh::loc::start().ok());
    CHECK(std::string(nnh::loc::code()) == "en");
    installed("ja-JP");
    CHECK(nnh::loc::start().ok());
    CHECK(std::string(nnh::loc::code()) == "ja");
}

NNH_TEST(loc_start_falls_back_to_the_base) {
    const StopAtEnd stop_at_end;
    installed("fr");
    CHECK(nnh::loc::start().ok());
    CHECK(std::string(nnh::loc::code()) == "en");
}

NNH_TEST(loc_start_never_picks_the_pseudo_language_by_itself) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    LocBuilder b = four_languages();
    b.add("qps", "Pseudo").key("menu.new_game", "[Ñéŵ ĝåɱé~~]");
    b.pseudo = 4;
    b.install();
    fake::set_system_language("qps");
    CHECK(nnh::loc::start().ok());
    CHECK(std::string(nnh::loc::code()) == "en");
    nnh::loc::stop();
    CHECK(nnh::loc::start({.language = "qps"}).ok()); // chosen on purpose: fine
    CHECK(std::string(nnh::loc::code()) == "qps");
}

NNH_TEST(loc_start_publishes_language_changed_once) {
    const StopAtEnd stop_at_end;
    installed("ru");
    g_changes = 0;
    auto sub = nnh::events::subscribe<nnh::LanguageChanged>([](const nnh::LanguageChanged& e) {
        ++g_changes;
        g_last = e;
    });
    CHECK(nnh::loc::start().ok());
    CHECK(nnh::frame());
    CHECK(g_changes == 1);
    CHECK(g_last.previous == nnh::loc::kNoLanguage);
    CHECK(g_last.current == 2);
    CHECK(g_last.serial == 1);
    CHECK(nnh::frame());
    CHECK(g_changes == 1);
}

NNH_TEST(loc_start_lists_the_shipped_languages) {
    const StopAtEnd stop_at_end;
    installed("en");
    CHECK(nnh::loc::start().ok());
    CHECK(nnh::loc::language_count() == 4);
    CHECK(std::string(nnh::loc::language_code(3)) == "ja");
    CHECK(std::string(nnh::loc::language_name(2)) == "Русский");
    CHECK(std::string(nnh::loc::language_code(4)).empty());
    CHECK(std::string(nnh::loc::language_name(-1)).empty());
    CHECK(nnh::loc::find("RU") == 2);
    CHECK(nnh::loc::find("de") == -1);
    CHECK(nnh::loc::find(nullptr) == -1);
}

NNH_TEST(loc_start_refuses_a_damaged_manifest_or_table_and_leaves_nothing) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    const LocBuilder b = four_languages();
    std::vector<std::uint8_t>& m = keep(b.manifest());
    m[m.size() - 2] ^= 1;
    for (const TestLanguage& l : b.languages) add_table_asset(l, keep(write_table(l)));
    test_assets().push_back({".nnh_loc", &m});
    remount();
    const int live = nnh::mem::live_allocations();
    auto r = nnh::loc::start();
    CHECK(!r.ok());
    if (!r) CHECK(r.error().code == nnh::ErrorCode::Corrupted);
    CHECK(nnh::mem::live_allocations() == live);

    fresh_engine(); // a table damaged: the manifest is fine
    std::vector<std::uint8_t>& t = keep(write_table(b.languages[0]));
    t[90] ^= 1;
    test_assets().push_back({"strings/en.nnhstr", &t});
    test_assets().push_back({".nnh_loc", &keep(b.manifest())});
    remount();
    fake::set_system_language("en");
    r = nnh::loc::start();
    CHECK(!r.ok());
    if (!r) CHECK(r.error().code == nnh::ErrorCode::Corrupted);
    CHECK(nnh::mem::live_allocations() == live);
    CHECK(nnh::loc::language_count() == 0);
}

NNH_TEST(loc_start_twice_asserts_and_changes_nothing) {
    const StopAtEnd stop_at_end;
    installed("en");
    CHECK(nnh::loc::start().ok());
    nnh::detail::crash_install();
    const bool crashed = fake::run_expecting_crash([] { (void)nnh::loc::start({.language = "ja"}); });
    CHECK(crashed == nnh::build::kAssertsEnabled);
    CHECK(std::string(nnh::loc::code()) == "en");
}

NNH_TEST(loc_stop_frees_everything_and_tr_is_empty_again) {
    const StopAtEnd stop_at_end;
    installed("en");
    const int live = nnh::mem::live_allocations();
    CHECK(nnh::loc::start().ok());
    CHECK(nnh::mem::live_allocations() == live + 2); // the manifest and the block
    nnh::loc::stop();
    CHECK(nnh::mem::live_allocations() == live);
    CHECK(std::string(nnh::loc::code()).empty());
    CHECK(nnh::loc::start().ok()); // and again
    CHECK(nnh::frame());
}
