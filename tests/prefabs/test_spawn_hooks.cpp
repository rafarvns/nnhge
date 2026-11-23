// Spawn hooks (spec 021 T4.4, FR-6, FR-4): called once per copy after its components, placement and tags, base first
// along a variant's chain, with the copy's SpawnInfo; hooks may spawn (this library or another), destroy (the new
// entity too: spawn returns its stale handle), add and remove — spec 020's rules.

#include "prefab_fixture.hpp"
#include "twin.hpp"

#include <string_view>

using nnh::Entity;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::SpawnInfo;
using nnh::World;
using namespace prefab_test;

namespace {

struct Log {
    char calls[16] = {};
    int count = 0;
    SpawnInfo info{};
    bool saw_body = false, saw_tag = false;
    void add(char c) {
        if (count < 15) calls[count++] = c;
    }
};

} // namespace

NNH_TEST(a_hook_sees_its_copy_in_place_and_its_spawn_info) {
    PrefabLibrary lib = make_library();
    Log log;
    Log* l = &log;
    const Prefab p = must_add(lib, "p",
                              {.tags = {kEnemy},
                               .place = nnh::place(&Body::box),
                               .on_spawn =
                                   [l](World& w, Entity e, const SpawnInfo& info) {
                                       l->add('p');
                                       l->info = info;
                                       const Body* b = w.get<Body>(e);
                                       l->saw_body = b != nullptr && b->box.x == 12 && b->layers == 3;
                                       l->saw_tag = w.has_tag(e, kEnemy);
                                   }},
                              Body{{0, 0, 4, 4}, 3});
    World w = make_world();
    lib.spawn(w, p, {12, 13});
    CHECK(log.count == 1 && log.saw_body && log.saw_tag);
    CHECK(log.info.has_position && log.info.position.x == 12 && log.info.position.y == 13);
    CHECK(log.info.object == nullptr && log.info.object_index == -1);
    lib.spawn(w, p);
    CHECK(log.count == 2 && !log.info.has_position);
}

NNH_TEST(a_variants_hooks_run_base_first) {
    PrefabLibrary lib = make_library();
    Log log;
    Log* l = &log;
    const Prefab a = must_add(lib, "a", {.on_spawn = [l](World&, Entity, const SpawnInfo&) { l->add('a'); }}, Coin{});
    const Prefab b = must_add(lib, "b", {.base = a}, Bob{});
    const Prefab c = must_add(lib, "c", {.base = b, .on_spawn = [l](World&, Entity, const SpawnInfo&) { l->add('c'); }});
    const Prefab d = must_add(lib, "d", {.base = c, .on_spawn = [l](World&, Entity, const SpawnInfo&) { l->add('d'); }});
    World w = make_world();
    lib.spawn(w, d);
    lib.spawn(w, b);
    CHECK(std::string_view(log.calls) == "acda");
}

NNH_TEST(hooks_may_spawn_from_this_library_and_another) {
    PrefabLibrary lib = make_library();
    PrefabLibrary other = make_library();
    const Prefab base = must_add(other, "base", {.tags = {kFx}}, Coin{9});
    struct Ctx {
        PrefabLibrary* lib;
        PrefabLibrary* other;
        Prefab child, base;
        Entity made[2];
    } ctx{&lib, &other, {}, base, {}};
    Ctx* c = &ctx;
    ctx.child = must_add(lib, "child", {}, Bob{1});
    const Prefab turret = must_add(lib, "turret", {.on_spawn = [c](World& w, Entity, const SpawnInfo&) {
                                                       c->made[0] = c->lib->spawn(w, c->child);
                                                       c->made[1] = c->other->spawn(w, c->base);
                                                   }},
                                   Body{});
    World w = make_world();
    const Entity t = lib.spawn(w, turret);
    CHECK(t.slot() == 0 && ctx.made[0].slot() == 1 && ctx.made[1].slot() == 2);
    CHECK(w.get<Bob>(ctx.made[0])->phase == 1 && w.has_tag(ctx.made[1], kFx));
    CHECK(lib.stats().spawns == 2 && other.stats().spawns == 1);
}

NNH_TEST(a_hook_that_destroys_its_copy_makes_spawn_return_a_stale_handle) {
    PrefabLibrary lib = make_library();
    const Prefab p = must_add(lib, "p", {.tags = {kFx}, .on_spawn = [](World& w, Entity e, const SpawnInfo&) {
                                             w.destroy(e);
                                         }},
                              Coin{1}, Bob{2});
    World a = make_world();
    World b = make_world();
    const Entity x = lib.spawn(a, p);
    const Entity y = b.spawn();
    b.add<Coin>(y, {1});
    b.add<Bob>(y, {2});
    b.add_tag(y, kFx);
    b.destroy(y);
    CHECK(x && x == y && !a.alive(x) && same_state(a, b));
    CHECK(lib.stats().spawns == 1);
}

NNH_TEST(hooks_may_destroy_add_and_remove_on_other_entities) {
    PrefabLibrary lib = make_library();
    World w = make_world();
    static Entity victim{};
    victim = w.spawn();
    w.add<Coin>(victim);
    const Prefab p = must_add(lib, "p", {.on_spawn = [](World& w, Entity e, const SpawnInfo&) {
                                             w.destroy(victim);
                                             w.add<Bob>(e, {4});
                                             w.remove<Coin>(e);
                                         }},
                              Coin{1});
    const Entity e = lib.spawn(w, p);
    CHECK(!w.alive(victim) && w.alive(e) && w.get<Bob>(e)->phase == 4 && !w.has<Coin>(e));
}
