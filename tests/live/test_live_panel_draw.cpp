// Drawing the pages (spec 030 T4.4, FR-7, FR-8): debug-list items at exact positions and texts for a group row, a
// float row (name, value right-aligned to x 262, `-`/`+`, the bar, the range), the changed mark and the pending mark; a
// full page stays within 90 entries.

#include <cstring>

#include "debug/debug_list.hpp"
#include "debug/dev_tools.hpp"
#include "live/live_apply.hpp"
#include "live/live_panel_layout.hpp"
#include "live/live_registry.hpp"
#include "live_fixture.hpp"
#include "nnh/debug.hpp"

using namespace nnh::detail::live;
using nnh::detail::DebugList;
using nnh::detail::DebugText;
using live_test::restart;
namespace live = nnh::live;

namespace {

/// The last frame's debug entry with this text, or -1.
int find_text(const char* t) {
    const DebugList& l = nnh::detail::debug_list_last_frame();
    for (int i = 0; i < l.count(); ++i) {
        if (l.at(i).kind == DebugText::Kind::Text && std::strcmp(l.text(i), t) == 0) return i;
    }
    return -1;
}

bool text_at(const char* t, int x, int y) {
    const int i = find_text(t);
    if (i < 0) {
        std::printf("    no text \"%s\"\n", t);
        return false;
    }
    const DebugText& e = nnh::detail::debug_list_last_frame().at(i);
    if (e.x != x || e.y != y) std::printf("    \"%s\" at (%d, %d)\n", t, e.x, e.y);
    return e.x == x && e.y == y && e.screen == nnh::Screen::Bottom;
}

} // namespace

NNH_TEST(live_panel_draw_rows_at_their_places) {
    if constexpr (!live::kEnabled) return;
    restart();
    float jump = 5.5f;
    live::Handle h = live::tune("player/jump", &jump, {.min = 0, .max = 12, .step = 0.25f});
    live::show_panel(live::Page::Tunables, "player/jump");
    live_test::frames(2);
    CHECK(text_at("Tunables 1/128", 4, kHeaderY));
    CHECK(text_at("- player", kNameX, row_y(0, kRowHeight) + 2));
    CHECK(text_at("jump", kNameX + 8, row_y(1, kRowHeight) + 2));
    CHECK(text_at("5.5", kValueRight - 3 * 8, row_y(1, kRowHeight) + 2));
    CHECK(text_at("-", kMinusX + 8, row_y(1, kRowHeight) + 2));
    CHECK(text_at("+", kPlusX + 8, row_y(1, kRowHeight) + 2));
    CHECK(text_at("0..12", kRangeX, row_y(1, kRowHeight) + kLine2Dy));
    CHECK(find_text("*") < 0 && find_text("~") < 0);
    jump = 6;
    live_test::frames(1);
    CHECK(text_at("*", 2, row_y(1, kRowHeight) + 2));
    CHECK(nnh::detail::debug_list_last_frame().count() <= 90);
}

NNH_TEST(live_panel_draw_marks_pending_values_while_paused) {
    if constexpr (!live::kEnabled) return;
    restart();
    float jump = 5.5f;
    live::Handle h = live::tune("jump", &jump, {.min = 0, .max = 12, .step = 0.25f});
    live::show_panel(live::Page::Tunables, "jump");
    live_test::frames(1);
    set_pending(*g_registry, id_slot(h.id()), float_bits(7.0f));
    live_test::frames(1); // applied at this frame's start... the pending mark shows while it waits:
    set_pending(*g_registry, id_slot(h.id()), float_bits(8.0f));
    nnh::detail::overlay_draw();
    nnh::detail::debug_text_flush();
    CHECK(text_at("~", 2, row_y(0, kRowHeight) + 2));
    CHECK(find_text("8") >= 0);
    CHECK(jump == 7.0f);
}

NNH_TEST(live_panel_draw_watches_and_an_empty_page) {
    if constexpr (!live::kEnabled) return;
    restart();
    live::show("top", 42);
    live::show_panel(live::Page::Watches);
    live_test::frames(2);
    live::show("top", 42);
    live_test::frames(1);
    CHECK(text_at("top", kNameX, row_y(0, kWatchRowHeight) + 1));
    CHECK(text_at("42", kWatchValueRight - 2 * 8, row_y(0, kWatchRowHeight) + 1));
    live::show_panel(live::Page::Tunables);
    live_test::frames(1);
    CHECK(find_text("no tunables") >= 0);
}
