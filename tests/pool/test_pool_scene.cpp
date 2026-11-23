// Scene-bound pools (spec 020 FR-18, D30; T5.1): flushed right before the scene's update and only while it is on top
// (frozen under a pushed scene), cleared right after on_exit, empty on re-entry; a bound run-time pool doesn't move.

#include "nnh/app.hpp"
#include "nnh/scene.hpp"
#include "pool_fixture.hpp"

using pool_test::Item;

namespace {

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
}

/// Each update acquires one item and releases the one of the previous update: with a flush before every update
/// the pool keeps reusing slots 0 and 1.
class Shooter : public nnh::Scene {
public:
    void update() override {
        if (last) pool.release(last);
        last = pool.acquire().handle;
        slots[updates % 8] = last.slot();
        ++updates;
    }
    void on_exit() override { left_with = pool.count(); }

    nnh::Pool<Item, 4> pool{*this};
    nnh::Handle<Item> last{};
    int slots[8] = {};
    int updates = 0;
    int left_with = -1;
};

class Idle : public nnh::Scene {};

} // namespace

NNH_TEST(a_bound_pool_is_flushed_right_before_its_scenes_update) {
    restart();
    {
        Shooter s;
        CHECK(s.pool.bound());
        nnh::scenes::push(s);
        frames(4);
        // update 1: slot 0; update 2: releases 0, takes 1 (0 quarantined); update 3: 0 again (flushed); ...
        CHECK(s.slots[0] == 0 && s.slots[1] == 1 && s.slots[2] == 0 && s.slots[3] == 1);
        nnh::shutdown();
    }
}

NNH_TEST(a_bound_pool_freezes_under_a_pushed_scene) {
    restart();
    {
        Shooter s;
        Idle pause;
        nnh::scenes::push(s);
        frames(1);
        s.pool.release(s.last); // slot 0 quarantined
        s.last = {};
        nnh::scenes::push(pause);
        frames(3); // not flushed: not the top scene
        CHECK(s.pool.acquire().handle.slot() == 1);
        nnh::scenes::pop();
        frames(2); // the pop applies at the start of the first frame; then flushed before each update
        CHECK(s.updates == 3);
        nnh::shutdown();
    }
}

NNH_TEST(a_bound_pool_is_cleared_after_on_exit_and_empty_on_re_entry) {
    restart();
    {
        Idle base;
        Shooter s;
        nnh::scenes::push(base);
        nnh::scenes::push(s);
        frames(3);
        nnh::scenes::pop();
        frames(1);
        CHECK(s.left_with == 1);       // on_exit still saw the item
        CHECK(s.pool.count() == 0);    // cleared right after it
        CHECK(s.pool.extent() == 0);
        s.last = {};
        nnh::scenes::push(s);
        frames(1);
        CHECK(s.slots[3] == 0); // back at slot 0
        nnh::shutdown();
    }
}

namespace {

nnh::Pool<Item>* g_bound = nullptr;
void move_a_bound_pool() {
    nnh::Pool<Item> other = static_cast<nnh::Pool<Item>&&>(*g_bound);
    CHECK(other.capacity() == 0); // nothing moved
}

} // namespace

NNH_TEST(a_bound_runtime_pool_does_not_move_and_detaches_when_destroyed) {
    restart();
    {
        Idle scene;
        nnh::Pool<Item> pool = pool_test::runtime<Item>(4);
        pool.bind(scene);
        nnh::scenes::push(scene);
        frames(1);
        g_bound = &pool;
        CHECK(pool_test::asserts_or_is_ignored(move_a_bound_pool));
        CHECK(pool.bound() && pool.capacity() == 4);
        pool = nnh::Pool<Item>{}; // destroying a bound pool detaches it (the scene may be destroyed later)
        CHECK(!pool.bound());
        nnh::shutdown();
    }
}
