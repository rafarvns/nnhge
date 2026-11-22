// The debug view (spec 017 T6.1, FR-24): enabled contexts and held actions by name as debug text in
// Debug/Development, inside a frame only; nothing in Release.

#include "actions_fixture.hpp"
#include "debug/debug_list.hpp"
#include "debug/dev_tools.hpp"
#include "nnh/debug.hpp"

#include <cstring>

using namespace actions_test;
using nnh::ActionSet;

namespace {
enum class Act { Jump, Fire, Back };
enum class Ctx { Play, Menu, Shop };

ActionSet make() {
    ActionSet set{{.actions = 3, .contexts = 3}};
    CHECK(set.add_context(Ctx::Play, "play"));
    CHECK(set.add_context(Ctx::Menu, "menu"));
    CHECK(set.add_context(Ctx::Shop, "shop", false));
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A)}}));
    CHECK(set.add(Act::Fire, {.name = "fire", .bindings = {btn(Button::B)}}));
    CHECK(set.add(Act::Back, {.name = "back", .context = Ctx::Menu, .bindings = {btn(Button::X)}}));
    return set;
}
} // namespace

NNH_TEST(the_debug_view_lists_enabled_contexts_and_held_actions) {
    restart();
    ActionSet set = make();
    frame();
    frame({.buttons = bit(Button::A) | bit(Button::X)});
    set.debug_draw(nnh::Screen::Bottom, 4, 8);
    CHECK(nnh::frame());
    const nnh::detail::DebugList& last = nnh::detail::debug_list_last_frame();
    if (nnh::build::kDevToolsEnabled) {
        CHECK(last.count() == 2);
        CHECK(std::strcmp(last.text(0), "actions: play menu") == 0);
        CHECK(std::strcmp(last.text(1), "held: jump back") == 0);
        CHECK(last.at(0).x == 4 && last.at(0).y == 8 && last.at(1).y == 8 + nnh::debug::line_height());
    } else {
        CHECK(last.count() == 0);
    }
    nnh::shutdown();
}

NNH_TEST(the_debug_view_says_when_nothing_is_held) {
    restart();
    ActionSet set = make();
    frame();
    set.debug_draw(nnh::Screen::Top, 0, 0);
    CHECK(nnh::frame());
    const nnh::detail::DebugList& last = nnh::detail::debug_list_last_frame();
    CHECK(!nnh::build::kDevToolsEnabled || (last.count() == 2 && std::strcmp(last.text(1), "held: -") == 0));
    nnh::shutdown();
}

NNH_TEST(the_debug_view_draws_nothing_outside_a_frame_or_for_an_empty_set) {
    restart();
    ActionSet set = make();
    set.debug_draw(nnh::Screen::Top, 0, 0); // before the first frame
    CHECK(nnh::frame());
    ActionSet().debug_draw(nnh::Screen::Top, 0, 0);
    CHECK(nnh::frame());
    CHECK(nnh::detail::debug_list_last_frame().count() == 0);
    nnh::shutdown();
}
