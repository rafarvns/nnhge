// Scene-bound worlds (spec 020 FR-18, FR-19, AC-3, D30; T5.2): the plan's proof as tests. Flushed right before the
// scene's update and only on top; cleared right after on_exit, before the next scene's on_enter, with hooks run and no
// spawn surviving; a hook asking for a scene change is queued; attach order with a bound TweenSet; a scene destroyed
// with a world still attached stops the game; a bound world doesn't move.

#include "entities/world_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/scene.hpp"
#include "nnh/tween.hpp"
#include "world_fixture.hpp"

#include <cstring>
#include <new>

using nnh::ComponentDesc;
using nnh::Entity;
using nnh::World;
using world_test::Hp;
using world_test::Pos;

namespace {

char g_calls[64];
int g_ncalls = 0;
void call(char c) {
    if (g_ncalls < 63) g_calls[g_ncalls++] = c;
    g_calls[g_ncalls] = 0;
}

void frames(int n) {
    for (int i = 0; i < n; ++i) {
        CHECK(nnh::frame());
        nnh::scenes::update_and_draw();
    }
}

void restart() {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({}));
    g_ncalls = 0;
    g_calls[0] = 0;
}

class Other : public nnh::Scene {
    void on_enter() override { call('E'); }
};

/// A level: its world's Hp hook logs 'h', tries to spawn and may request a scene change.
class Level : public nnh::Scene {
public:
    Level() {
        const ComponentDesc kinds[] = {nnh::component<Hp>(8, [this](World& w, Entity, Hp&) {
                                           call('h');
                                           if (w.clearing()) spawned_in_clear = spawned_in_clear || static_cast<bool>(w.spawn());
                                           if (replace_from_hook != nullptr && w.clearing()) {
                                               nnh::scenes::replace(*replace_from_hook); // once
                                               replace_from_hook = nullptr;
                                           }
                                       }),
                                       nnh::component<Pos>(8)};
        world = static_cast<World&&>(World::create({.max_entities = 8}, kinds).value());
        world.bind(*this);
    }
    void on_enter() override {
        call('E');
        for (int i = 0; i < 3; ++i) world.add<Hp>(world.spawn());
    }
    void on_exit() override { call('X'); }
    void update() override {
        ++updates;
        if (updates == 1) world.destroy(world.storage<Hp>().owner(0)); // quarantines entity slot 0
        if (updates == 2) first_free = world.spawn().slot();
    }

    World world;
    int updates = 0;
    int first_free = -1;
    bool spawned_in_clear = false;
    nnh::Scene* replace_from_hook = nullptr;
};

} // namespace

NNH_TEST(a_bound_world_is_flushed_right_before_its_scenes_update) {
    restart();
    {
        Level level;
        CHECK(level.world.bound());
        nnh::scenes::push(level);
        frames(2);
        CHECK(level.first_free == 0); // slot 0 destroyed in update 1, flushed before update 2
        nnh::shutdown();
    }
}

NNH_TEST(a_bound_world_freezes_under_a_pushed_scene) {
    restart();
    {
        Level level;
        Other pause;
        nnh::scenes::push(level);
        frames(1);
        nnh::scenes::push(pause);
        frames(3);
        CHECK(level.world.spawn().slot() == 3); // still quarantined: no flush under the pause
        nnh::shutdown();
    }
}

NNH_TEST(a_bound_world_is_cleared_after_on_exit_and_before_the_next_on_enter) {
    restart();
    {
        Level level;
        Other next;
        nnh::scenes::push(level);
        frames(1);
        g_ncalls = 0;
        nnh::scenes::replace(next);
        frames(1);
        CHECK(std::strcmp(g_calls, "XhhE") == 0); // exit, the two remaining hooks, then the next scene
        CHECK(!level.spawned_in_clear);
        CHECK(level.world.count() == 0);
        nnh::scenes::replace(level); // entered again: empty, slots from 0
        frames(1);
        CHECK(level.world.storage<Hp>().owner(0).slot() == 0 && level.world.storage<Hp>().count() == 3);
        nnh::shutdown();
    }
}

NNH_TEST(a_hook_asking_for_a_scene_change_during_the_exit_clear_is_queued) {
    restart();
    {
        Level level;
        Other a, b;
        level.replace_from_hook = &b;
        nnh::scenes::push(level);
        frames(1);
        nnh::scenes::replace(a);
        frames(2);
        CHECK(nnh::scenes::top() == &b); // a entered, then the queued request replaced it
        nnh::shutdown();
    }
}

namespace {

/// A world and a tween set bound to one scene; the Hp hook kills the tween of its entity (or starts one).
class Tweened : public nnh::Scene {
public:
    explicit Tweened(bool world_first) {
        const ComponentDesc kinds[] = {nnh::component<Hp>(4, [this](World& w, Entity, Hp&) {
            if (w.clearing()) {
                killed_running = tweens->running(handle);
                tweens->kill(handle);
            }
        })};
        world = static_cast<World&&>(World::create({.max_entities = 4}, kinds).value());
        if (world_first) world.bind(*this);
        tweens = new nnh::TweenSet(*this, {.tweens = 4});
        if (!world_first) world.bind(*this);
    }
    ~Tweened() override { delete tweens; }
    void on_enter() override {
        world.add<Hp>(world.spawn());
        handle = tweens->to(&x, 10.0f, nnh::Frames{100});
    }

    World world;
    nnh::TweenSet* tweens = nullptr;
    nnh::TweenHandle handle{};
    float x = 0;
    bool killed_running = false;
};

} // namespace

NNH_TEST(a_world_bound_before_a_tween_set_runs_its_hooks_first) {
    restart();
    {
        Tweened first(true), second(false);
        Other other;
        nnh::scenes::push(first);
        frames(2);
        nnh::scenes::replace(second);
        frames(2);
        CHECK(first.killed_running);  // the world's hooks saw the tween still running, then killed it
        nnh::scenes::replace(other);
        frames(1);
        CHECK(!second.killed_running); // bound after: the set was cleared first; kill() of a stale handle is harmless
        CHECK(second.tweens->active() == 0 && first.tweens->active() == 0);
        nnh::shutdown();
    }
}

namespace {

struct Bare : nnh::Scene {};
World* g_outliving = nullptr;

void destroy_a_scene_with_a_world_attached() {
    // Static storage, not new/delete: the crash fires inside ~Scene, so a delete would never free (LeakSanitizer).
    alignas(Bare) static unsigned char storage[sizeof(Bare)];
    Bare* scene = ::new (static_cast<void*>(storage)) Bare;
    g_outliving->bind(*scene);
    scene->~Bare(); // the world is not a member: still attached
}

World* g_bound = nullptr;
void move_a_bound_world() {
    World other = static_cast<World&&>(*g_bound);
    CHECK(other.capacity() == 0);
}

} // namespace

NNH_TEST(destroying_a_scene_with_a_world_still_attached_stops_the_game) {
    restart();
    World w = world_test::make_world();
    g_outliving = &w;
    CHECK(world_test::crashes(destroy_a_scene_with_a_world_attached)); // NNH_VERIFY: every flavor
    nnh::detail::WorldHot* hot = nnh::WorldAccess::hot(w);
    hot->cold->scene = nullptr; // the scene is gone: forget it without touching its list
    nnh::shutdown();
}

NNH_TEST(a_bound_world_does_not_move) {
    restart();
    {
        Level level;
        g_bound = &level.world;
        CHECK(world_test::asserts_or_is_ignored(move_a_bound_world));
        CHECK(level.world.bound() && level.world.capacity() == 8);
        nnh::shutdown();
    }
}
