// Opt-in bus events (spec 028 T4.8, FR-20): none by default; with set_events(true) the results are published at the
// end of the update and delivered at the next frame's dispatch, with keys and values; each event is at most 16 bytes.

#include "nnh/events.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiWidget;
using ui_test::K;

namespace {

struct Seen {
    int activated = 0, changed = 0, focus = 0, cancelled = 0;
    int value = -1;
    nnh::UiKey key{};
};
Seen g_seen;

} // namespace

NNH_TEST(ui_events_are_at_most_16_bytes) {
    CHECK(sizeof(nnh::UiActivated) <= 16 && sizeof(nnh::UiValueChanged) <= 16);
    CHECK(sizeof(nnh::UiFocusChanged) <= 16 && sizeof(nnh::UiCancelled) <= 16);
}

NNH_TEST(ui_events_are_off_by_default) {
    ui_test::restart();
    {
        g_seen = Seen{};
        nnh::Subscription s = nnh::events::subscribe<nnh::UiActivated>([](const nnh::UiActivated&) { ++g_seen.activated; });
        Ui ui = ui_test::make();
        ui.button(ui.root(Screen::Bottom), {.text = "a"});
        ui_test::settle(ui);
        ui_test::tap(ui, ui_test::kConfirm);
        CHECK(nnh::frame());
        CHECK(nnh::frame());
        CHECK(g_seen.activated == 0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_events_are_delivered_at_the_next_dispatch) {
    ui_test::restart();
    {
        g_seen = Seen{};
        nnh::Subscription a = nnh::events::subscribe<nnh::UiActivated>([](const nnh::UiActivated& e) {
            ++g_seen.activated;
            g_seen.key = e.key;
        });
        nnh::Subscription v = nnh::events::subscribe<nnh::UiValueChanged>([](const nnh::UiValueChanged& e) {
            ++g_seen.changed;
            g_seen.value = e.value;
        });
        nnh::Subscription f = nnh::events::subscribe<nnh::UiFocusChanged>([](const nnh::UiFocusChanged&) { ++g_seen.focus; });
        nnh::Subscription c = nnh::events::subscribe<nnh::UiCancelled>([](const nnh::UiCancelled&) { ++g_seen.cancelled; });
        Ui ui = ui_test::make();
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
        ui.button(box, {.text = "a", .key = K::A});
        ui.slider(box, {.text = "s", .value = 2, .key = K::Slider});
        ui.set_events(true);
        CHECK(nnh::frame());
        ui_test::settle(ui);
        ui_test::tap(ui, ui_test::kConfirm);
        CHECK(g_seen.activated == 0); // published, not yet delivered
        CHECK(nnh::frame());
        CHECK(g_seen.activated == 1 && g_seen.key == nnh::UiKey(K::A));
        ui_test::tap(ui, ui_test::kDown);
        ui_test::tap(ui, ui_test::kRight);
        ui_test::tap(ui, ui_test::kCancel);
        CHECK(nnh::frame());
        CHECK(g_seen.focus == 1 && g_seen.changed == 1 && g_seen.value == 3 && g_seen.cancelled == 1);
    }
    nnh::shutdown();
}
