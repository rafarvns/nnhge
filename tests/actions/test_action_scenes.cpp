// Scene-bound contexts (spec 017 T3.4, FR-5, FR-13, AC-3): enabled exactly while their scene is on top, switched
// from the frame after a push or pop, the press that pops a menu doesn't reach gameplay, the binding ends when
// the scene leaves the stack; misuse asserts.

#include "actions_fixture.hpp"
#include "nnh/scene.hpp"

#include <new>

using namespace actions_test;
using nnh::ActionSet;

namespace {
enum class Act { Jump, Confirm };
enum class Ctx { Play, Menu };

ActionSet* g_set = nullptr;

class Level : public nnh::Scene {
public:
    void on_enter() override { g_set->bind_context(Ctx::Play, *this); }
    void update() override {
        if (g_set->pressed(Act::Jump)) ++jumps;
    }
    int jumps = 0;
};

class Menu : public nnh::Scene {
public:
    void on_enter() override { g_set->bind_context(Ctx::Menu, *this); }
    void update() override {
        if (g_set->pressed(Act::Confirm)) {
            ++confirms;
            nnh::scenes::pop();
        }
    }
    int confirms = 0;
};

ActionSet make() {
    ActionSet set{{.actions = 2, .contexts = 2}};
    CHECK(set.add_context(Ctx::Menu, "menu", false));
    CHECK(set.add(Act::Jump, {.name = "jump", .context = Ctx::Play, .bindings = {btn(Button::A)}}));
    CHECK(set.add(Act::Confirm, {.name = "confirm", .context = Ctx::Menu, .bindings = {btn(Button::A)}}));
    return set;
}

void play(int n, const Pad& p = {}) {
    for (int i = 0; i < n; ++i) {
        frame(p);
        nnh::scenes::update_and_draw();
    }
}

alignas(ActionSet) unsigned char g_storage[sizeof(ActionSet)];
void destroy_bound_set() { reinterpret_cast<ActionSet*>(g_storage)->~ActionSet(); }

Level* g_level = nullptr;
void bind_off_stack() { g_set->bind_context(Ctx::Play, *g_level); }
} // namespace

NNH_TEST(a_bound_context_is_enabled_only_while_its_scene_is_on_top) {
    restart();
    ActionSet set = make();
    g_set = &set;
    {
        Level level;
        Menu menu;
        nnh::scenes::push(level);
        play(1);
        CHECK(set.enabled(Ctx::Play) && !set.enabled(Ctx::Menu));
        play(1, {.buttons = bit(Button::A)});
        CHECK(level.jumps == 1);
        nnh::scenes::push(menu);
        play(1); // the menu is on top from this frame's evaluation
        CHECK(!set.enabled(Ctx::Play) && set.enabled(Ctx::Menu));
        play(1, {.buttons = bit(Button::A)}); // confirm pops the menu (applied next frame)
        CHECK(menu.confirms == 1 && level.jumps == 1);
        play(3, {.buttons = bit(Button::A)}); // A still held: blocked for gameplay
        CHECK(level.jumps == 1 && set.enabled(Ctx::Play) && !set.enabled(Ctx::Menu));
        play(1);
        play(1, {.buttons = bit(Button::A)});
        CHECK(level.jumps == 2);
        nnh::shutdown();
    }
}

NNH_TEST(a_binding_ends_when_its_scene_leaves_the_stack) {
    restart();
    ActionSet set = make();
    g_set = &set;
    {
        Level level;
        Menu menu;
        nnh::scenes::push(level);
        nnh::scenes::push(menu);
        play(1);
        CHECK(nnh::detail::SceneAccess::head(menu) != nullptr);
        nnh::scenes::pop();
        play(1);
        CHECK(nnh::detail::SceneAccess::head(menu) == nullptr);
        CHECK(block_of(set)->ctx[1].scene == nullptr && !set.enabled(Ctx::Menu));
        nnh::scenes::push(menu); // re-entering binds again (on_enter)
        play(1);
        CHECK(set.enabled(Ctx::Menu) && !set.enabled(Ctx::Play));
        nnh::shutdown(); // every scene exits: every binding ends
        CHECK(block_of(set)->ctx[0].scene == nullptr && block_of(set)->ctx[1].scene == nullptr);
    }
}

NNH_TEST(unbinding_a_context_makes_it_manual_and_disabled) {
    restart();
    ActionSet set = make();
    g_set = &set;
    {
        Level level;
        nnh::scenes::push(level);
        play(1);
        set.unbind_context(Ctx::Play);
        CHECK(!set.enabled(Ctx::Play) && nnh::detail::SceneAccess::head(level) == nullptr);
        set.enable(Ctx::Play, true);
        CHECK(set.enabled(Ctx::Play));
        nnh::shutdown();
    }
}

NNH_TEST(destroying_a_set_bound_to_a_scene_on_the_stack_asserts_and_detaches) {
    restart();
    {
        Level level;
        ActionSet* set = new (g_storage) ActionSet(make());
        g_set = set;
        nnh::scenes::push(level);
        play(1);
        CHECK(asserts_or_is_ignored(destroy_bound_set));
        CHECK(nnh::detail::SceneAccess::head(level) == nullptr); // detached either way
        nnh::shutdown();
    }
}

NNH_TEST(binding_a_scene_that_is_not_on_the_stack_asserts) {
    restart();
    ActionSet set = make();
    g_set = &set;
    Level level;
    g_level = &level;
    CHECK(asserts_or_is_ignored(bind_off_stack));
    CHECK(block_of(set)->ctx[0].scene == nullptr);
    nnh::shutdown();
}
