// Component overrides per copy (spec 021 T4.2, FR-7): an override replaces the prefab's value of its kind (constructed
// once, from the override) or adds a kind after the prefab's entries; twin worlds agree. The array form asserts on a
// kind given twice and ignores the second.
//
// Compile-time refusals (kept as documentation):
//   lib.spawn_with(w, p, Coin{1}, Coin{2});          // the same override type twice
//   lib.spawn_with(w, p, nnh::Vec2{1, 2});           // a bare Vec2 override reads as a position: no overload
//   lib.spawn_with(w, p, at, Coin{1}, nnh::Vec2{});  // a bare Vec2 among the overrides

#include "prefab_fixture.hpp"
#include "twin.hpp"

using nnh::ComponentValue;
using nnh::Entity;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::World;
using namespace prefab_test;

namespace {

struct Fixture {
    PrefabLibrary lib = make_library();
    Prefab coin = must_add(lib, "coin", {.tags = {kCoinTag}}, Body{{0, 0, 8, 8}, 2}, Coin{1});
    World a = make_world();
    World b = make_world();
};

} // namespace

NNH_TEST(an_override_replaces_the_prefabs_value_for_one_copy) {
    Fixture f;
    const Entity gold = f.lib.spawn_with(f.a, f.coin, Coin{5});
    const Entity plain = f.lib.spawn(f.a, f.coin);
    const Entity g = f.b.spawn();
    f.b.add<Body>(g, {{0, 0, 8, 8}, 2});
    f.b.add<Coin>(g, {5});
    f.b.add_tag(g, kCoinTag);
    const Entity p = f.b.spawn();
    f.b.add<Body>(p, {{0, 0, 8, 8}, 2});
    f.b.add<Coin>(p, {1});
    f.b.add_tag(p, kCoinTag);
    CHECK(gold == g && plain == p && same_state(f.a, f.b));
    CHECK(f.a.get<Coin>(gold)->value == 5 && f.lib.value<Coin>(f.coin)->value == 1); // the prefab is unchanged
}

NNH_TEST(an_override_of_a_kind_the_prefab_lacks_is_added_after_its_entries) {
    Fixture f;
    const Entity e = f.lib.spawn_with(f.a, f.coin, Bob{9}, Coin{3});
    const Entity t = f.b.spawn();
    f.b.add<Body>(t, {{0, 0, 8, 8}, 2});
    f.b.add<Coin>(t, {3});
    f.b.add<Bob>(t, {9});
    f.b.add_tag(t, kCoinTag);
    CHECK(e == t && same_state(f.a, f.b) && f.a.get<Bob>(e)->phase == 9);
    CHECK(!f.a.has<Bob>(f.lib.spawn(f.a, f.coin))); // that copy only
}

NNH_TEST(an_override_is_constructed_once_from_the_override) {
    Counted::reset();
    {
        PrefabLibrary lib = make_library();
        Counted stored;
        stored.id = 1;
        const Prefab p = must_add(lib, "p", {}, stored);
        World w = make_world();
        Counted over;
        over.id = 2;
        const int before = Counted::constructed;
        const Entity e = lib.spawn_with(w, p, over);
        CHECK(Counted::constructed == before + 1 && w.get<Counted>(e)->id == 2);
    }
    CHECK(Counted::alive() == 0);
}

NNH_TEST(the_array_form_takes_overrides_and_a_position) {
    Fixture f;
    PrefabLibrary lib = make_library();
    const Prefab p = must_add(lib, "p", {.place = nnh::place(&Body::box)}, Body{{0, 0, 8, 8}, 2}, Coin{1});
    const Coin c{6};
    const ComponentValue values[] = {nnh::component_value(c)};
    const nnh::Vec2 at{10, 20};
    const Entity e = lib.spawn_with(f.a, p, &at, values, 1);
    CHECK(f.a.get<Coin>(e)->value == 6 && f.a.get<Body>(e)->box.x == 10 && f.a.get<Body>(e)->box.y == 20);
    const Entity n = lib.spawn_with(f.a, p, nullptr, nullptr, 0);
    CHECK(f.a.get<Coin>(n)->value == 1 && f.a.get<Body>(n)->box.x == 0);
}

namespace {
Fixture* g_f = nullptr;
Entity g_e{};
void override_twice() {
    const Coin first{2}, second{3};
    const ComponentValue values[] = {nnh::component_value(first), nnh::component_value(second)};
    g_e = g_f->lib.spawn_with(g_f->a, g_f->coin, nullptr, values, 2);
}
void add_twice() {
    const Bob first{2}, second{3};
    const ComponentValue values[] = {nnh::component_value(first), nnh::component_value(second)};
    g_e = g_f->lib.spawn_with(g_f->a, g_f->coin, nullptr, values, 2);
}
} // namespace

NNH_TEST(a_kind_overridden_twice_asserts_and_the_second_is_ignored) {
    Fixture f;
    g_f = &f;
    g_e = {};
    CHECK(asserts_or_is_ignored(override_twice));
    if (!nnh::build::kAssertsEnabled) CHECK(f.a.get<Coin>(g_e)->value == 2);
    g_e = {};
    CHECK(asserts_or_is_ignored(add_twice));
    if (!nnh::build::kAssertsEnabled) CHECK(f.a.get<Bob>(g_e)->phase == 2);
    g_f = nullptr;
}
