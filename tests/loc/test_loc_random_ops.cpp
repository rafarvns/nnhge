// 100 000 seeded operations (spec 029 FR-25, FR-34, FR-35): switches (shipped, unknown, repeated, several per frame),
// following the system language, lookups and formats of known and unknown keys, frames. Invariants: tr() never null and
// always the current language's text or its chain's, the serial counts the switches that took effect, nothing is
// allocated after start.

#include <string>

#include "loc_fixture.hpp"
#include "nnh/localization.hpp"
#include "nnh/memory.hpp"
#include "test.hpp"

using namespace loc_test;
using namespace font_test;

NNH_TEST(loc_random_operations_keep_the_invariants) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    four_languages().install();
    nnh::platform_fake::set_system_language("en");
    CHECK(nnh::loc::start().ok());
    const char* codes[] = {"en", "pt-BR", "ru", "ja", "fr", "", "PT-br"};
    const char* systems[] = {"en-US", "ru", "ja-JP", "pt-PT", "de"};
    const char* expected_new_game[] = {"New game", "Novo jogo", "Новая игра", "ニューゲーム"};
    const nnh::loc::Key keys[] = {"menu.new_game", "menu.quit", "items.count", "hud.score", "no.such"};
    const int live = nnh::mem::live_allocations();
    std::uint32_t state = 2029u;
    int pending = -1, current = nnh::loc::index(), failures = 0, switches = 0;
    bool following = false;
    char out[64];
    for (int i = 0; i < 100000; ++i) {
        state = state * 1664525u + 1013904223u;
        switch (state >> 29) {
        case 0: {
            const char* c = codes[(state >> 8) % 7];
            const bool ok = nnh::loc::set_language(c).ok();
            if (ok) {
                pending = nnh::loc::find(c);
                following = false; // the player's choice turns following off
            }
            failures += ok == (nnh::loc::find(c) < 0) ? 1 : 0;
            break;
        }
        case 1:
            nnh::platform_fake::set_system_language(systems[(state >> 8) % 5]);
            if (((state >> 12) & 15u) == 0) {
                nnh::loc::follow_system(true);
                following = true;
            }
            break;
        case 2:
        case 3: {
            const std::uint32_t serial = nnh::loc::serial();
            CHECK(nnh::frame());
            const int now = nnh::loc::index();
            // the serial moves by one exactly when the language changed
            failures += nnh::loc::serial() - serial == (now != current ? 1u : 0u) ? 0 : 1;
            // without following, the language is the player's last choice (or unchanged)
            if (!following) failures += now == (pending >= 0 ? pending : current) ? 0 : 1;
            switches += now != current ? 1 : 0;
            pending = -1;
            current = now;
            failures += std::string(nnh::loc::tr("menu.new_game")) == expected_new_game[current] ? 0 : 1;
            break;
        }
        default: {
            const nnh::loc::Key k = keys[(state >> 8) % 5];
            failures += nnh::loc::tr(k) == nullptr ? 1 : 0;
            const int n = nnh::loc::format(out, sizeof out, k, nnh::loc::arg("count", static_cast<int>(state >> 3) - 100000),
                                           nnh::loc::arg("score", static_cast<long long>(state) << 31));
            failures += n < static_cast<int>(sizeof out) && out[n] == 0 ? 0 : 1;
            break;
        }
        }
    }
    CHECK(failures == 0);
    CHECK(switches > 100);
    CHECK(nnh::mem::live_allocations() == live);
}
