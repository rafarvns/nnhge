// The platformer's coin sparkles (examples/platformer/sparkles.hpp, spec 020 AC-4 step 2): four "fx" entities per
// coin, faded by a tween through a handle-capturing setter and destroyed from the tween's own callback; slots reused
// only after a flush; a clear leaves no tween running.

#include "nnh/tween.hpp"
#include "platformer/level_world.hpp"
#include "platformer/sparkles.hpp"
#include "test.hpp"

using level::Spark;
using nnh::Entity;
using nnh::World;

namespace {

/// The tween set first: the world's Spark hook kills tweens in it, and members go in reverse order.
struct Fixture {
    nnh::TweenSet tweens{{.tweens = 64}};
    World world = static_cast<World&&>(level::create_world(&tweens).value());
    level::LevelPrefabs prefabs = static_cast<level::LevelPrefabs&&>(level::create_level_prefabs(&tweens).value());
    Sparkles sparkles{world, prefabs};

    int count() { return world.storage<Spark>().count(); }
    void frames(int n) {
        for (int i = 0; i < n; ++i) {
            sparkles.update();
            tweens.update();
        }
    }
};

} // namespace

NNH_TEST(a_burst_spawns_four_tagged_sparkles_at_the_coins_centre) {
    Fixture f;
    f.sparkles.burst({100, 50});
    CHECK(f.count() == Sparkles::kPerCoin);
    CHECK(f.world.find(level::kFxTag, nullptr, 0) == 4);
    int at_centre = 0;
    for (auto [e, s] : f.world.storage<Spark>().items()) {
        at_centre += s.position.x == 100 && s.position.y == 50 ? 1 : 0;
        CHECK(s.alpha == 1.0f && f.world.has<level::Velocity>(e));
    }
    CHECK(at_centre == 4);
}

NNH_TEST(sparkles_fly_apart_and_fade) {
    Fixture f;
    f.sparkles.burst({100, 50});
    f.frames(12);
    float left = 1e9f, right = -1e9f;
    for (auto [e, s] : f.world.storage<Spark>().items()) {
        left = s.position.x < left ? s.position.x : left;
        right = s.position.x > right ? s.position.x : right;
        CHECK(s.alpha > 0.0f && s.alpha < 1.0f);
        CHECK(s.position.y < 50);
    }
    CHECK(left < 100 && right > 100);
}

NNH_TEST(sparkles_are_gone_after_their_fade_destroyed_them) {
    Fixture f;
    f.sparkles.burst({100, 50});
    f.frames(Sparkles::kFadeFrames - 1);
    CHECK(f.count() == 4);
    f.frames(1);
    CHECK(f.count() == 0 && f.world.count() == 0);
    CHECK(f.tweens.active() == 0);
}

NNH_TEST(sparkle_slots_are_reused_only_after_a_flush) {
    Fixture f;
    f.sparkles.burst({100, 50});
    const Entity first = f.world.storage<Spark>().owner(0);
    f.frames(Sparkles::kFadeFrames);
    f.sparkles.burst({10, 10}); // same frame as their destruction: other slots
    CHECK(f.world.storage<Spark>().owner(4).slot() == 4 && !f.world.storage<Spark>().is_live(0));
    f.frames(Sparkles::kFadeFrames);
    f.world.flush();
    f.sparkles.burst({10, 10});
    const Entity again = f.world.storage<Spark>().owner(0);
    CHECK(again.slot() == first.slot() && again.generation() == first.generation() + 1);
}

NNH_TEST(a_cleared_world_leaves_no_sparkle_tween_running) {
    Fixture f;
    f.sparkles.burst({100, 50});
    f.sparkles.burst({140, 50});
    f.frames(3);
    CHECK(f.tweens.active() == 8);
    f.world.clear(); // a level restart: the Spark hook kills each tween
    CHECK(f.tweens.active() == 0 && f.count() == 0);
}

NNH_TEST(each_sparkle_moves_by_its_own_velocity) {
    // Mutation testing (plan 020 T8.3): the four directions, exactly (short binary fractions).
    Fixture f;
    f.sparkles.burst({100, 50});
    f.frames(4);
    const nnh::Vec2 want[4] = {{97, 45}, {103, 45}, {98.5f, 43}, {101.5f, 43}};
    int i = 0;
    for (auto [e, s] : f.world.storage<Spark>().items()) CHECK(i < 4 && s.position == want[i++]);
    CHECK(i == 4);
}
