// The frames' cull reach (spec 024 FR-19; T10.3): an emitter remembers its frames' reach per texture size, so
// one culled whole reads no frame; the remembered reach follows set_desc and the texture's size, and a frame
// outside the texture is still reported when the emitter is off screen.

#include "gfx/particle_batches.hpp"
#include "particles/particle_block.hpp"
#include "particles_fixture.hpp"

namespace nnh::detail {
ParticleExpandResult particle_expand(const ParticleExpandRequest& q); // particle_expand.cpp
}

using nnh::ParticleDesc;
using nnh::ParticleEmitter;
using nnh::Vec2;
using nnh::detail::ParticleExpandRequest;
using nnh::detail::ParticleExpandResult;

namespace {

/// One expansion of `e` on the top screen at zoom 1, textured with a `w` × `h` texture.
ParticleExpandResult expand(const ParticleEmitter& e, int w, int h, Vec2 camera = {}) {
    static nnh::platform::DrawItem2D items[16];
    static std::uint32_t frame = 0;
    ParticleExpandRequest q;
    q.source = nnh::ParticleAccess::block(e);
    q.camera = camera;
    q.textured = true;
    q.texture = 1;
    q.texture_w = w;
    q.texture_h = h;
    q.frame = ++frame;
    q.out = items;
    q.capacity = 16;
    return nnh::detail::particle_expand(q);
}

ParticleDesc framed(nnh::Region region) {
    ParticleDesc d;
    d.frames[0].region = region;
    return d;
}

} // namespace

NNH_TEST(a_frame_outside_the_texture_is_reported_even_for_an_emitter_off_screen) {
    ParticleEmitter e = particles_test::make(framed({0, 0, 40, 40}), 8);
    e.emit_at(Vec2{-900, 100}, 3);
    const ParticleExpandResult fits = expand(e, 64, 64);
    CHECK(!fits.bad_frame && fits.culled == 3 && fits.written == 0);
    CHECK(expand(e, 32, 32).bad_frame); // the remembered 64 x 64 answer is not reused
    CHECK(!expand(e, 64, 64).bad_frame);
    CHECK(expand(e, 64, 32).bad_frame);
}

NNH_TEST(set_desc_frames_change_the_cull_reach) {
    ParticleEmitter e = particles_test::make(framed({0, 0, 4, 4}), 8); // reach 2 + 2 + 2 px of rounding
    e.emit_at(Vec2{-20, 100}, 1);
    CHECK(expand(e, 64, 64).culled == 1);
    e.set_desc(framed({0, 0, 32, 16})); // reach 16 + 8 + 2: the particle's quad may touch the view
    const ParticleExpandResult r = expand(e, 64, 64);
    CHECK(r.culled == 0 && r.written == 1);
}

NNH_TEST(a_whole_texture_frame_reaches_as_far_as_the_texture_is_large) {
    ParticleEmitter e = particles_test::make(framed({}), 8);
    e.emit_at(Vec2{-20, 100}, 1);
    CHECK(expand(e, 8, 8).culled == 1);    // reach 4 + 4 + 2
    CHECK(expand(e, 64, 64).written == 1); // reach 32 + 32 + 2
    CHECK(expand(e, 8, 8).culled == 1);
}
