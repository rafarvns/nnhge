// Interop by data layout (spec 020 FR-20, AC-3; T5.3): spec 019's box views read a body storage in place (live, free
// and quarantined slots; only live bodies are reported and owner() maps a box index back to its entity); a sprite
// animator is a component updated in place whose callback destroys its entity; a tween writes a component through a
// handle-capturing setter and stops after destroy; a pointer-targeted tween is killed by the kind's remove hook.
// Neither module includes the other: only this test sees both.

#include "nnh/animation.hpp"
#include "nnh/collision.hpp"
#include "nnh/tween.hpp"
#include "world_fixture.hpp"

using nnh::ComponentDesc;
using nnh::Entity;
using nnh::World;
namespace collision = nnh::collision;

namespace {

constexpr collision::Layers kEnemy = collision::layer(3);

struct Body {
    nnh::Rect box{};
    collision::Layers layers = collision::kNoLayers; // free and quarantined slots: on no layer
};

World body_world() {
    const ComponentDesc kinds[] = {nnh::component<Body>(16)};
    return static_cast<World&&>(World::create({.max_entities = 16}, kinds).value());
}

collision::BoxView view_of(nnh::Storage<Body> bodies) {
    return collision::box_view(bodies.data(), bodies.extent(), &Body::box, &Body::layers);
}

} // namespace

NNH_TEST(box_views_over_a_body_storage_report_only_live_bodies) {
    World w = body_world();
    Entity es[4];
    for (int i = 0; i < 4; ++i) {
        es[i] = w.spawn();
        w.add<Body>(es[i], {{static_cast<float>(i * 20), 0, 16, 16}, kEnemy});
    }
    w.destroy(es[1]); // quarantined: its slot holds Body{}
    w.destroy(es[3]);
    w.flush();         // free
    w.destroy(es[2]); // quarantined again
    const nnh::Storage<Body> bodies = w.storage<Body>();
    int found[4];
    CHECK(collision::boxes_overlapping(view_of(bodies), {0, 0, 100, 16}, found, 4, {.mask = kEnemy}) == 1);
    CHECK(bodies.owner(found[0]) == es[0]);
    CHECK(collision::boxes_at(view_of(bodies), {25, 8}, found, 4) == 0); // es[1]'s old place
    collision::RayHit hit;
    CHECK(collision::raycast(view_of(bodies), {100, 8}, {-10, 8}, hit, {.mask = kEnemy}));
    CHECK(bodies.owner(hit.box) == es[0]);
}

namespace {

/// A game object owning a world whose animators destroy their entity when their clip finishes.
struct Game {
    World world;
    int destroyed = 0;

    void watch(Entity e, nnh::SpriteAnimator& a) {
        a.on_notice([this, e](nnh::SpriteAnimator&, const nnh::SpriteNotice& n) {
            if (n.kind != nnh::SpriteNoticeKind::Finished) return;
            // This lambda lives inside the animator it destroys: once destroy() returns, the slot holds a fresh
            // animator and the captures are gone. Copy what is needed first; destroying is the last thing it does.
            Game* game = this;
            ++game->destroyed;
            game->world.destroy(e);
        });
    }
};

} // namespace

NNH_TEST(an_animator_component_updates_in_place_and_may_destroy_its_entity) {
    auto lib = nnh::SpriteLibrary::create({.frames = 4, .clips = 2, .clip_frames = 8});
    CHECK(lib.ok());
    nnh::SpriteLibrary& library = lib.value();
    CHECK(library.add_grid({.frame_width = 16, .frame_height = 16, .count = 4, .columns = 4}).ok());
    auto clip = library.add_clip({.name = "pop", .count = 2, .frame_time = nnh::Frames{2},
                                  .mode = nnh::SpriteMode::Once});
    CHECK(clip.ok());
    const ComponentDesc kinds[] = {nnh::component<nnh::SpriteAnimator>(4)};
    Game game{static_cast<World&&>(World::create({.max_entities = 4}, kinds).value())};
    const Entity e = game.world.spawn();
    nnh::SpriteAnimator* a = game.world.add<nnh::SpriteAnimator>(e, nnh::SpriteAnimator(library));
    game.watch(e, *a);
    a->play(clip.value());
    for (int i = 0; i < 8; ++i)
        game.world.each<nnh::SpriteAnimator>([](Entity, nnh::SpriteAnimator& anim) { anim.update(); });
    CHECK(game.destroyed == 1);
    CHECK(!game.world.alive(e));
    CHECK(!a->clip().valid()); // the slot holds a fresh animator
}

namespace {

struct Fade {
    float alpha = 1;
};
struct Glow {
    float level = 0;
    nnh::TweenHandle tween{};
};

} // namespace

NNH_TEST(a_setter_capturing_the_world_and_a_handle_stops_writing_after_destroy) {
    const ComponentDesc kinds[] = {nnh::component<Fade>(4)};
    World w = static_cast<World&&>(World::create({.max_entities = 4}, kinds).value());
    World* wp = &w;
    const Entity e = w.spawn();
    Fade* f = w.add<Fade>(e);
    nnh::TweenSet tweens({.tweens = 4});
    tweens.from_to(nnh::Setter<float>{[wp, e](float v) {
                       if (Fade* x = wp->get<Fade>(e)) x->alpha = v;
                   }},
                   1.0f, 0.0f, nnh::Frames{10});
    for (int i = 0; i < 5; ++i) tweens.update();
    CHECK(f->alpha == 0.5f);
    w.destroy(e);
    for (int i = 0; i < 5; ++i) tweens.update();
    CHECK(f->alpha == 1.0f); // Fade{} in the slot, never written again
}

NNH_TEST(a_pointer_tween_on_a_component_is_killed_by_its_remove_hook) {
    static nnh::TweenSet* set = nullptr;
    nnh::TweenSet tweens({.tweens = 4});
    set = &tweens;
    const ComponentDesc kinds[] = {
        nnh::component<Glow>(4, [](World&, Entity, Glow& g) { set->kill(g.tween); })};
    World w = static_cast<World&&>(World::create({.max_entities = 4}, kinds).value());
    const Entity e = w.spawn();
    Glow* g = w.add<Glow>(e);
    g->tween = tweens.to(&g->level, 10.0f, nnh::Frames{10});
    tweens.update();
    CHECK(g->level == 1.0f);
    w.destroy(e);
    CHECK(tweens.active() == 0);
    tweens.update();
    CHECK(g->level == 0.0f); // the slot's Glow{} untouched
}
