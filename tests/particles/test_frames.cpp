// Sprite frames (spec 024 FR-13; T3.2): first, random (uniform), over life with repeats, at a rate; the reciprocal
// rule for m mod n exact over its whole domain.

#include "particles/particle_appearance.hpp"
#include "particles_fixture.hpp"

using nnh::Frames;
using nnh::ParticleDesc;
using nnh::ParticleEmitter;
using nnh::ParticleFrameMode;

namespace {

ParticleDesc with_frames(int n, ParticleFrameMode mode) {
    ParticleDesc d;
    for (int i = 0; i < n; ++i) d.frames[i].region = {8 * i, 0, 8, 8};
    d.frame_mode = mode;
    d.lifetime = {Frames{80}, Frames{80}};
    return d;
}

} // namespace

NNH_TEST(frame_mod_reciprocal_is_exact_over_its_domain) {
    for (std::uint32_t n = 1; n <= 16; ++n) {
        const std::uint32_t recip = ((1u << 28) + n - 1) / n;
        bool ok = true;
        for (std::uint32_t m = 0; m < (1u << 24); ++m) ok = ok && nnh::detail::frame_mod(m, n, recip) == m % n;
        CHECK(ok);
    }
}

NNH_TEST(first_frame_mode_shows_frame_zero) {
    ParticleEmitter e = particles_test::make(with_frames(4, ParticleFrameMode::First), 8);
    e.emit(3);
    for (int f = 0; f < 70; ++f) e.update();
    for (int i = 0; i < 3; ++i) CHECK(e.particle(i).frame == 0);
}

NNH_TEST(random_frames_are_uniform_and_kept) {
    ParticleEmitter e = particles_test::make(with_frames(5, ParticleFrameMode::Random), 8192);
    e.emit(5000);
    int counts[5] = {};
    for (int i = 0; i < 5000; ++i) ++counts[e.particle(i).frame];
    for (int c : counts) CHECK(c > 1000 - 140 && c < 1000 + 140);
    const int before = e.particle(17).frame;
    for (int f = 0; f < 30; ++f) e.update();
    CHECK(e.particle(17).frame == before);
}

NNH_TEST(over_life_frames_play_first_to_last_with_repeats) {
    ParticleDesc d = with_frames(4, ParticleFrameMode::OverLife);
    ParticleEmitter once = particles_test::make(d, 4);
    d.frame_repeats = 2;
    ParticleEmitter twice = particles_test::make(d, 4);
    once.emit(1);
    twice.emit(1);
    int seen_once[80], seen_twice[80];
    for (int f = 0; f < 80; ++f) {
        seen_once[f] = once.particle(0).frame;
        seen_twice[f] = twice.particle(0).frame;
        if (f < 79) {
            once.update();
            twice.update();
        }
    }
    // 80 frames of life: 20 per frame once, 10 per frame twice (checked away from the exact boundaries).
    CHECK(seen_once[0] == 0 && seen_once[19] == 0 && seen_once[21] == 1 && seen_once[41] == 2 && seen_once[61] == 3);
    CHECK(seen_once[79] == 3);
    CHECK(seen_twice[9] == 0 && seen_twice[11] == 1 && seen_twice[41] == 0 && seen_twice[79] == 3);
}

NNH_TEST(rate_frames_advance_per_second_and_wrap) {
    ParticleDesc d = with_frames(3, ParticleFrameMode::Rate);
    d.frame_rate = 6; // a frame every 10 updates
    ParticleEmitter e = particles_test::make(d, 4);
    e.emit(1);
    int seen[61];
    for (int f = 0; f <= 60; ++f) {
        seen[f] = e.particle(0).frame;
        if (f < 60) e.update();
    }
    CHECK(seen[0] == 0 && seen[9] == 0 && seen[11] == 1 && seen[21] == 2 && seen[31] == 0 && seen[59] == 2);
}

NNH_TEST(without_frames_every_particle_shows_frame_zero) {
    ParticleDesc d;
    d.frame_mode = ParticleFrameMode::Random;
    ParticleEmitter e = particles_test::make(d, 64);
    e.emit(64);
    for (int i = 0; i < 64; ++i) CHECK(e.particle(i).frame == 0);
}
