// The panel's touch (spec 030 T4.3, FR-9): row hits, group rows toggling, a drag tick by tick through a bar with the
// rounding rule, `-`/`+` with repeat while held, bool and action hits, a touch starting outside the rows.

#include "debug/dev_controls.hpp"
#include "live/live_apply.hpp"
#include "live/live_panel_layout.hpp"
#include "live/live_panel_model.hpp"
#include "live/live_registry.hpp"
#include "live_fixture.hpp"

using namespace nnh::detail::live;
using nnh::detail::DevInput;
using live_test::restart;
namespace live = nnh::live;

namespace {

DevInput touch(int x, int y, bool started) {
    DevInput in;
    in.touching = true;
    in.touch_started = started;
    in.touch_x = x;
    in.touch_y = y;
    return in;
}
DevInput lift() {
    DevInput in;
    in.touch_ended = true;
    return in;
}
void feed(const DevInput& in) { panel_touch(*g_registry, in); }
std::uint32_t pending_of(const live::Handle& h) { return tunable_of(*g_registry, h.id())->pending; }

} // namespace

NNH_TEST(live_panel_touch_a_tap_selects_a_row_and_toggles_a_group) {
    if constexpr (!live::kEnabled) return;
    restart();
    float a = 0;
    bool b = false;
    live::Handle ha = live::tune("p/a", &a, {.min = 0, .max = 1, .step = 0.25f});
    live::Handle hb = live::tune("q", &b);
    // rows: p (closed), q
    feed(touch(20, row_y(1, kRowHeight) + 3, true));
    CHECK(panel_selected(*g_registry, kPageTunables) == 1);
    feed(lift());
    feed(touch(20, row_y(0, kRowHeight) + 3, true)); // the group row: opens it
    feed(lift());
    CHECK(row_count(*g_registry, kPageTunables) == 3);
}

NNH_TEST(live_panel_touch_drags_through_a_bar_on_the_grid) {
    if constexpr (!live::kEnabled) return;
    restart();
    float a = 0;
    live::Handle ha = live::tune("a", &a, {.min = 0, .max = 1, .step = 0.25f}); // depth 0: bar x 8..178 (w 170)
    const int y = row_y(0, kRowHeight) + kBarDy + 2;
    feed(touch(8, y, true));
    CHECK(pending_of(ha) == float_bits(0.0f));
    feed(touch(8 + 64, y, false)); // k = (64·4 + 85) / 170 = 2
    CHECK(pending_of(ha) == float_bits(0.5f));
    feed(touch(8 + 63, y, false)); // k = (63·4 + 85) / 170 = 1
    CHECK(pending_of(ha) == float_bits(0.25f));
    feed(touch(300, y, false)); // past the bar's end: clamped to max... the drag keeps the bar's rule
    CHECK(pending_of(ha) == float_bits(1.0f));
    feed(lift());
}

NNH_TEST(live_panel_touch_minus_and_plus_repeat_while_held) {
    if constexpr (!live::kEnabled) return;
    restart();
    std::int32_t n = 10;
    live::Handle h = live::tune("n", &n, {.min = 0, .max = 100, .step = 1});
    const int y = row_y(0, kRowHeight) + 4;
    feed(touch(kPlusX + 4, y, true));
    CHECK(pending_of(h) == 11);
    for (int t = 0; t < 23; ++t) feed(touch(kPlusX + 4, y, false));
    CHECK(pending_of(h) == 11);
    feed(touch(kPlusX + 4, y, false)); // tick 24
    CHECK(pending_of(h) == 12);
    for (int t = 0; t < 6; ++t) feed(touch(kPlusX + 4, y, false));
    CHECK(pending_of(h) == 13);
    feed(lift());
    feed(touch(kMinusX + 4, y, true));
    CHECK(pending_of(h) == 12);
    feed(lift());
}

NNH_TEST(live_panel_touch_bools_toggle_and_actions_run) {
    if constexpr (!live::kEnabled) return;
    restart();
    bool b = false;
    int runs = 0;
    live::Handle hb = live::tune("b", &b);
    live::Handle ha = live::action("go", [](void* c) { ++*static_cast<int*>(c); }, &runs); // rows: b, go
    feed(touch(220, row_y(0, kRowHeight) + 4, true));
    feed(lift());
    CHECK(pending_of(hb) == 1);
    feed(touch(kMinusX + 20, row_y(1, kRowHeight) + 4, true));
    feed(lift());
    live_test::frames(1);
    CHECK(runs == 1 && b);
}

NNH_TEST(live_panel_touch_starting_outside_the_rows_does_nothing) {
    if constexpr (!live::kEnabled) return;
    restart();
    float a = 0;
    live::Handle ha = live::tune("a", &a, {.min = 0, .max = 1, .step = 0.25f});
    feed(touch(20, 5, true)); // the header
    feed(touch(8 + 100, row_y(0, kRowHeight) + kBarDy + 2, false));
    feed(lift());
    CHECK((tunable_of(*g_registry, ha.id())->flags & kPending) == 0);
}
