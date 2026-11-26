// The panel's model (spec 030 T4.2, FR-5, FR-8, Q9): rows and groups, navigation (wrap only on fresh presses, repeat),
// steps (×10 with L or R), the max stop, colour channels, enum ends, bools, actions once per press, X on an entry and
// on a group, Y, B, selection kept when an entry goes.

#include <cstring>

#include "debug/dev_controls.hpp"
#include "live/live_apply.hpp"
#include "live/live_panel_model.hpp"
#include "live/live_registry.hpp"
#include "live_fixture.hpp"
#include "nnh/gfx.hpp"
#include "nnh/input.hpp"

using namespace nnh::detail::live;
using nnh::detail::DevInput;
using live_test::restart;
namespace live = nnh::live;

namespace {

constexpr std::uint32_t bit(nnh::Button b) { return static_cast<std::uint32_t>(b); }

DevInput press(std::uint32_t b, std::uint32_t held_too = 0) {
    DevInput in;
    in.held = b | held_too;
    in.pressed = b;
    in.repeated = b;
    return in;
}
DevInput repeat(std::uint32_t b) {
    DevInput in;
    in.held = b;
    in.repeated = b;
    return in;
}

void buttons(const DevInput& in) { panel_buttons(*g_registry, kPageTunables, in); }
Row selected_row() {
    Row row;
    CHECK(row_at(*g_registry, kPageTunables, panel_selected(*g_registry, kPageTunables), row));
    return row;
}
std::uint32_t pending_of(const live::Handle& h) { return tunable_of(*g_registry, h.id())->pending; }

enum class Shape : std::uint8_t { Circle, Square, Diamond };
constexpr const char* kShapes[] = {"circle", "square", "diamond"};

struct Fixture {
    float jump = 5.5f, gravity = 0.3f;
    bool hitboxes = false;
    nnh::Color sky{10, 20, 30, 255};
    Shape shape = Shape::Circle;
    int runs = 0;
    live::Handle hj, hg, hb, hc, hs, ha;
    Fixture() {
        hb = live::tune("debug/hitboxes", &hitboxes);
        hc = live::tune("colors/sky", &sky);
        hj = live::tune("player/jump", &jump, {.min = 0, .max = 12, .step = 0.25f});
        hg = live::tune("player/gravity", &gravity, {.min = 0.05f, .max = 1, .step = 0.01f});
        hs = live::tune("spawn/shape", &shape, kShapes);
        ha = live::action("player/respawn", [](void* c) { ++*static_cast<int*>(c); }, &runs);
    }
};

/// Selects the entry `name` (groups opened).
void select(const char* name) { panel_select_name(*g_registry, kPageTunables, name); }

} // namespace

NNH_TEST(live_panel_rows_start_with_closed_groups) {
    if constexpr (!live::kEnabled) return;
    restart();
    Fixture f;
    CHECK(row_count(*g_registry, kPageTunables) == 4); // colors, debug, player, spawn
    Row row;
    CHECK(row_at(*g_registry, kPageTunables, 2, row));
    CHECK(row.type == RowType::Group && !row.open && row.count == 3);
    int n = 0;
    const char* label = row_label(row, n);
    CHECK(n == 6 && std::memcmp(label, "player", 6) == 0);
}

NNH_TEST(live_panel_a_opens_and_b_closes_a_group) {
    if constexpr (!live::kEnabled) return;
    restart();
    Fixture f;
    buttons(press(bit(nnh::Button::Down)));
    buttons(press(bit(nnh::Button::Down)));
    buttons(press(bit(nnh::Button::A))); // open player
    CHECK(row_count(*g_registry, kPageTunables) == 7);
    buttons(press(bit(nnh::Button::Down)));
    CHECK(selected_row().type == RowType::Tunable);
    buttons(press(bit(nnh::Button::B))); // closes player, back on its row
    CHECK(row_count(*g_registry, kPageTunables) == 4);
    CHECK(panel_selected(*g_registry, kPageTunables) == 2);
}

NNH_TEST(live_panel_navigation_wraps_only_on_a_fresh_press) {
    if constexpr (!live::kEnabled) return;
    restart();
    Fixture f;
    buttons(repeat(bit(nnh::Button::Up)));
    CHECK(panel_selected(*g_registry, kPageTunables) == 0);
    buttons(press(bit(nnh::Button::Up)));
    CHECK(panel_selected(*g_registry, kPageTunables) == 3);
    buttons(repeat(bit(nnh::Button::Down)));
    CHECK(panel_selected(*g_registry, kPageTunables) == 3);
    buttons(press(bit(nnh::Button::Down)));
    CHECK(panel_selected(*g_registry, kPageTunables) == 0);
}

NNH_TEST(live_panel_steps_by_one_and_ten_and_stops_at_max) {
    if constexpr (!live::kEnabled) return;
    restart();
    Fixture f;
    select("player/jump");
    buttons(press(bit(nnh::Button::Right)));
    CHECK(pending_of(f.hj) == float_bits(5.75f));
    buttons(press(bit(nnh::Button::Right), bit(nnh::Button::L)));
    CHECK(pending_of(f.hj) == float_bits(8.25f));
    buttons(press(bit(nnh::Button::Right), bit(nnh::Button::R)));
    CHECK(pending_of(f.hj) == float_bits(10.75f));
    for (int i = 0; i < 10; ++i) buttons(repeat(bit(nnh::Button::Right)));
    CHECK(pending_of(f.hj) == float_bits(12.0f));
    buttons(press(bit(nnh::Button::Left)));
    CHECK(pending_of(f.hj) == float_bits(11.75f));
    CHECK(f.jump == 5.5f); // nothing written before the application point
    live_test::frames(1);
    CHECK(f.jump == 11.75f);
}

NNH_TEST(live_panel_colour_channels_cycle_with_a) {
    if constexpr (!live::kEnabled) return;
    restart();
    Fixture f;
    select("colors/sky");
    buttons(press(bit(nnh::Button::Right)));
    CHECK(pending_of(f.hc) == pack_rgba(11, 20, 30, 255));
    buttons(press(bit(nnh::Button::A)));
    buttons(press(bit(nnh::Button::Right), bit(nnh::Button::L)));
    CHECK(pending_of(f.hc) == pack_rgba(11, 30, 30, 255));
    buttons(press(bit(nnh::Button::A)));
    buttons(press(bit(nnh::Button::A)));
    buttons(press(bit(nnh::Button::Right)));
    CHECK(pending_of(f.hc) == pack_rgba(11, 30, 30, 255)); // alpha stops at 255
    buttons(press(bit(nnh::Button::A)));
    CHECK(g_registry->panel.channel == 0);
}

NNH_TEST(live_panel_enums_stop_at_their_ends_and_bools_toggle) {
    if constexpr (!live::kEnabled) return;
    restart();
    Fixture f;
    select("spawn/shape");
    buttons(press(bit(nnh::Button::Left)));
    CHECK(pending_of(f.hs) == 0);
    buttons(press(bit(nnh::Button::Right), bit(nnh::Button::R)));
    CHECK(pending_of(f.hs) == 2);
    select("debug/hitboxes");
    buttons(press(bit(nnh::Button::A)));
    CHECK(pending_of(f.hb) == 1);
    buttons(press(bit(nnh::Button::Right)));
    CHECK(pending_of(f.hb) == 0);
}

NNH_TEST(live_panel_actions_run_once_per_press) {
    if constexpr (!live::kEnabled) return;
    restart();
    Fixture f;
    select("player/respawn");
    buttons(press(bit(nnh::Button::A)));
    DevInput held;
    held.held = bit(nnh::Button::A);
    buttons(held);
    live_test::frames(1);
    CHECK(f.runs == 1);
}

NNH_TEST(live_panel_x_resets_an_entry_or_a_group_and_y_asks_for_a_dump) {
    if constexpr (!live::kEnabled) return;
    restart();
    Fixture f;
    f.jump = 7;
    f.gravity = 0.5f;
    select("player/jump");
    buttons(press(bit(nnh::Button::X)));
    live_test::frames(1);
    CHECK(f.jump == 5.5f && f.gravity == 0.5f);
    buttons(press(bit(nnh::Button::B))); // on the player group row
    buttons(press(bit(nnh::Button::X)));
    live_test::frames(1);
    CHECK(f.gravity == 0.3f);
    buttons(press(bit(nnh::Button::Y)));
    CHECK(g_registry->dump_requested);
}

NNH_TEST(live_panel_selection_stays_in_range_when_an_entry_goes) {
    if constexpr (!live::kEnabled) return;
    restart();
    Fixture f;
    select("spawn/shape"); // the last row
    const int before = panel_selected(*g_registry, kPageTunables);
    f.hs = live::Handle{};
    buttons(DevInput{});
    CHECK(panel_selected(*g_registry, kPageTunables) < before);
    CHECK(panel_selected(*g_registry, kPageTunables) < row_count(*g_registry, kPageTunables));
}

NNH_TEST(live_panel_select_chord_leaves_the_panel_alone) {
    if constexpr (!live::kEnabled) return;
    restart();
    Fixture f;
    select("player/jump");
    buttons(press(bit(nnh::Button::Right), bit(nnh::Button::Select)));
    CHECK((tunable_of(*g_registry, f.hj.id())->flags & kPending) == 0);
}
