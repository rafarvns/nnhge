// Clock edges found by the mutation pass (spec 024 T8.1): a looping duration that isn't a whole number of frames keeps
// its remainder, a looping burst-only emitter is never finished, start() while emitting changes nothing.

#include "particles_fixture.hpp"

using nnh::Frames;
using nnh::ParticleDesc;
using nnh::ParticleEmitter;

NNH_TEST(looping_duration_keeps_the_part_of_a_frame_past_its_end) {
    ParticleDesc d;
    d.duration = nnh::FrameTime::from_ticks(4096 * 10 + 2048); // 10.5 frames
    d.looping = true;
    d.lifetime = {Frames{1}, Frames{1}};
    d.bursts[0] = {.count = {1, 1}}; // once per loop, at its start
    ParticleEmitter e = particles_test::make(d, 8);
    int fired = 0;
    for (int f = 0; f < 42; ++f) {
        e.update();
        fired += e.alive();
    }
    // Loops start at 0, 10.5, 21 and 31.5 frames: bursts in the updates starting at 0, 11, 21 and 32.
    CHECK(fired == 4);
    ParticleEmitter again = particles_test::make(d, 8);
    int frame_of_third = -1, seen = 0;
    for (int f = 0; f < 42 && frame_of_third < 0; ++f) {
        again.update();
        if (again.alive() > 0 && ++seen == 3) frame_of_third = f;
    }
    CHECK(frame_of_third == 21); // the third loop starts at 21.0 frames: its burst is in the frame [21, 22)
}

NNH_TEST(looping_burst_only_emitter_is_never_finished) {
    ParticleDesc d;
    d.duration = Frames{20};
    d.looping = true;
    d.lifetime = {Frames{2}, Frames{2}};
    d.bursts[0] = {.count = {2, 2}};
    ParticleEmitter e = particles_test::make(d, 8);
    for (int f = 0; f < 15; ++f) e.update();
    CHECK(e.alive() == 0 && !e.finished()); // between bursts: the next loop fires again
}

NNH_TEST(start_while_emitting_changes_nothing) {
    ParticleDesc d;
    d.rate = 60;
    d.delay = Frames{3};
    d.lifetime = {Frames{100}, Frames{100}};
    ParticleEmitter e = particles_test::make(d, 64);
    for (int f = 0; f < 5; ++f) e.update();
    CHECK(e.alive() == 2);
    e.start(); // already emitting: the delay is not waited again
    e.update();
    CHECK(e.alive() == 3);
}
