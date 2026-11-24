// The hierarchy's debug draw (spec 022 FR-22, T4.7): in Debug/Development a line from each non-root node to its
// parent and two axes per node (the fake's recorded lines); in Release nothing.

#include "../gfx/gfx_fixture.hpp"
#include "hierarchy_fixture.hpp"
#include "nnh/gfx.hpp"

namespace fake = nnh::platform_fake;
using nnh::Entity;
using nnh::Hierarchy;
using nnh::Screen;
using nnh::World;
using namespace hierarchy_test;
using Kind = nnh::platform::DrawItem2D::Kind;

NNH_TEST(debug_draw_lines_to_parents_and_axes) {
    gfx_test::restart();
    World w = make_world();
    Hierarchy h(w);
    const Entity a = spawn_node(w, {.position = {20, 20}});
    const Entity b = spawn_node(w, {.position = {10, 0}});
    h.attach(b, a, nnh::Reparent::KeepLocal);
    CHECK(nnh::frame());
    h.debug_draw(Screen::Bottom);
    CHECK(nnh::frame());
    const int expected = nnh::build::kDevToolsEnabled ? 1 + 2 * 2 : 0;
    CHECK(fake::state().last_frame_item_count == expected);
    int lines = 0;
    for (int i = 0; i < fake::state().last_frame_item_count; ++i)
        lines += fake::state().last_frame_items[i].kind == Kind::Line ? 1 : 0;
    CHECK(lines == expected);
    nnh::shutdown();
}
