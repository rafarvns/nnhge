// Misuse (spec 024 FR-25; T3.6): every listed misuse asserts in Debug/Development and is harmless in Release (the
// pc-release run of these tests): commands on empty or moved-from emitters, non-finite transforms and points, negative
// counts, particle() out of range.

#include <cmath>
#include <initializer_list>

#include "particles_fixture.hpp"

using nnh::ParticleEmitter;
using nnh::Vec2;

namespace {

ParticleEmitter* g_e = nullptr;
int g_n = 0;

void update_empty() {
    ParticleEmitter empty;
    empty.update();
}
void emit_empty() {
    ParticleEmitter empty;
    g_n = empty.emit(3);
}
void emit_moved_from() {
    ParticleEmitter a = particles_test::make({}, 4);
    ParticleEmitter b = static_cast<ParticleEmitter&&>(a);
    g_n = a.emit(1);
}
void emit_negative() { g_n = g_e->emit(-2); }
void emit_at_nan() { g_n = g_e->emit_at(Vec2{NAN, 0}, 1); }
void emit_at_bad_transform() {
    nnh::WorldTransform2D t;
    t.rotation = INFINITY;
    g_n = g_e->emit_at(t, 1);
}
void transform_nan() { g_e->set_transform(nnh::Transform2D{.position = {0, NAN}}); }
void world_transform_inf() {
    nnh::WorldTransform2D t;
    t.map.x.x = INFINITY;
    g_e->set_transform(t);
}
void position_nan() { g_e->set_position({NAN, 1}); }
void particle_out_of_range() { g_n = g_e->particle(g_e->alive()).frame + 7; }
void particle_negative() { g_n = g_e->particle(-1).frame + 7; }
void stop_empty() {
    ParticleEmitter empty;
    empty.stop();
    empty.start();
    empty.clear();
    empty.restart();
}
void bind_empty() {
    ParticleEmitter empty;
    nnh::Scene* s = nullptr;
    empty.unbind();
    (void)s;
}

} // namespace

NNH_TEST(particle_misuse_asserts_and_is_harmless_in_release) {
    ParticleEmitter e = particles_test::make({}, 8);
    e.set_position({3, 4});
    e.emit(2);
    g_e = &e;
    for (auto fn : {update_empty, emit_empty, emit_moved_from, emit_negative, emit_at_nan, emit_at_bad_transform,
                    transform_nan, world_transform_inf, position_nan, particle_out_of_range, particle_negative,
                    stop_empty, bind_empty}) {
        g_n = 0;
        CHECK(particles_test::asserts_or_is_ignored(fn));
        if (!nnh::build::kAssertsEnabled) CHECK(g_n == 0 || g_n == 7); // nothing made; a zero view
    }
    CHECK(e.alive() == 2 && e.transform().position() == (Vec2{3, 4}));
}
