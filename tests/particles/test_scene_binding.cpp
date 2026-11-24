// Scene binding (spec 024 FR-17, D30 unchanged; T3.5): a bound emitter is updated right before its scene's update,
// frozen under a pushed scene, cleared right after on_exit; moved and destroyed while bound; bind twice is ignored.

#include "nnh/app.hpp"
#include "nnh/scene.hpp"
#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;

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

ParticleDesc one_per_frame() {
    ParticleDesc d;
    d.rate = 60;
    d.lifetime = {nnh::Frames{1000}, nnh::Frames{1000}};
    return d;
}

/// Records the emitter's live count seen by each update (the bound update runs first).
class Fountain : public nnh::Scene {
public:
    Fountain() : fx(particles_test::make(one_per_frame(), 512)) { fx.bind(*this); }
    void update() override { seen[updates++ % 16] = fx.alive(); }
    void on_exit() override { left_with = fx.alive(); }
    ParticleEmitter fx;
    int seen[16] = {};
    int updates = 0;
    int left_with = -1;
};

class Pause : public nnh::Scene {};

ParticleEmitter* g_e = nullptr;
nnh::Scene* g_scene = nullptr;
void bind_twice() { g_e->bind(*g_scene); }

} // namespace

NNH_TEST(bound_emitter_updates_right_before_its_scene) {
    restart();
    {
        Fountain s;
        nnh::scenes::push(s);
        frames(3);
        CHECK(s.seen[0] == 1 && s.seen[1] == 2 && s.seen[2] == 3);
        nnh::shutdown();
        CHECK(s.fx.alive() == 0); // cleared after on_exit
        CHECK(s.left_with == 3);
    }
}

NNH_TEST(bound_emitter_freezes_under_a_pause_scene) {
    restart();
    {
        Fountain s;
        Pause p;
        nnh::scenes::push(s);
        frames(2);
        nnh::scenes::push(p);
        frames(5);
        CHECK(s.fx.alive() == 2);
        nnh::scenes::pop();
        frames(2);
        CHECK(s.fx.alive() == 4);
        nnh::shutdown();
    }
}

NNH_TEST(bound_emitter_is_cleared_after_its_scene_leaves) {
    restart();
    {
        Fountain s;
        Pause other;
        nnh::scenes::push(s);
        frames(4);
        nnh::scenes::replace(other);
        frames(1);
        CHECK(s.left_with == 4 && s.fx.alive() == 0);
        nnh::shutdown();
    }
}

NNH_TEST(bound_emitter_moved_and_destroyed_while_bound) {
    restart();
    {
        Pause scene;
        nnh::scenes::push(scene);
        ParticleEmitter a = particles_test::make(one_per_frame(), 64);
        a.bind(scene);
        frames(2);
        ParticleEmitter b = static_cast<ParticleEmitter&&>(a); // the block (and its attachment) moves with it
        frames(2);
        CHECK(b.alive() == 4);
        {
            ParticleEmitter c = particles_test::make(one_per_frame(), 64);
            c.bind(scene);
            frames(1);
        } // destroyed while bound: detached
        frames(1);
        CHECK(b.alive() == 6);
        g_e = &b;
        g_scene = &scene;
        CHECK(particles_test::asserts_or_is_ignored(bind_twice));
        frames(1);
        CHECK(b.alive() == 7); // still one update per frame
        b.unbind();
        frames(1);
        CHECK(b.alive() == 7);
        nnh::shutdown();
    }
}
