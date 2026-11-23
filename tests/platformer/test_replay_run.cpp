// The platformer's replay bookkeeping (examples/platformer/replay_run.hpp, spec 012 T7.3): scripted input,
// checksum, segments, checkpoints and the 60 Hz meters.

#include "platformer/replay_run.hpp"
#include "test.hpp"

#include <cstdint>
#include <cstring>

namespace {

constexpr PlayerInput kRight{.run = 1};
constexpr PlayerInput kJumpRight{.run = 1, .jump = true};
constexpr PlayerInput kDrop{.jump = true, .down = true};

constexpr ReplayStep kSteps[] = {
    {.frames = 2, .input = kRight}, {.frames = 3, .input = kJumpRight}, {.frames = 1, .input = kDrop}};

/// Two segments: 120 frames of level 1, then 60 of the bonus level.
ReplayRun two_segments() {
    static const ReplaySegment segments[] = {{Demo::Level1, kSteps, 3, 120}, {Demo::Bonus, kSteps, 3, 60}};
    return ReplayRun(segments, 2);
}

ReplaySample sample() {
    return {.box = {37, 162.5f, 10, 14}, .camera = {0, 80}, .touching = {.floor = true}, .coins = 3,
            .turrets = 0x0BADF00Du, .entities = 7, .handles = 0x01234567u};
}

/// Records `frames` frames of `ms` each; returns the last progress.
ReplayProgress play(ReplayRun& run, int frames, double ms = 16.7) {
    ReplayProgress last;
    for (int i = 0; i < frames; ++i)
        last = run.record(sample(), ms);
    return last;
}

} // namespace

NNH_TEST(replay_input_holds_each_step_for_its_frames) {
    CHECK(replay_input(kSteps, 3, 0) == kRight);
    CHECK(replay_input(kSteps, 3, 1) == kRight);
    CHECK(replay_input(kSteps, 3, 2).run == 1);
}

NNH_TEST(replay_input_jumps_only_on_the_first_frame_of_a_step) {
    CHECK(replay_input(kSteps, 3, 2) == kJumpRight);
    CHECK(replay_input(kSteps, 3, 3) == kRight);
    CHECK(replay_input(kSteps, 3, 4) == kRight);
    CHECK(replay_input(kSteps, 3, 5) == kDrop);
}

NNH_TEST(replay_input_is_idle_past_the_last_step) {
    CHECK(replay_input(kSteps, 3, 6) == PlayerInput{});
    CHECK(replay_input(kSteps, 3, 1000) == PlayerInput{});
    CHECK(replay_input(nullptr, 0, 0) == PlayerInput{});
}

NNH_TEST(replay_checksum_is_fnv1a_of_the_sample_words) {
    // Python: FNV-1a over struct.pack('<I', w) of [f32(37), f32(162.5), f32(0), f32(80), 8, 3, 0x0BADF00D, 7,
    // 0x01234567].
    CHECK(checksum_add(kChecksumStart, sample()) == 0x5807D4BBu);
}

NNH_TEST(replay_checksum_sees_a_one_ulp_change_of_a_position) {
    ReplaySample moved = sample();
    std::uint32_t bits;
    std::memcpy(&bits, &moved.box.x, 4);
    ++bits;
    std::memcpy(&moved.box.x, &bits, 4);
    CHECK(checksum_add(kChecksumStart, moved) != checksum_add(kChecksumStart, sample()));
}

NNH_TEST(replay_checksum_sees_the_sides_coins_and_camera) {
    const std::uint32_t base = checksum_add(kChecksumStart, sample());
    ReplaySample s = sample();
    s.touching.left = true;
    CHECK(checksum_add(kChecksumStart, s) != base);
    s = sample();
    s.coins = 4;
    CHECK(checksum_add(kChecksumStart, s) != base);
    s = sample();
    s.camera.x = 1;
    CHECK(checksum_add(kChecksumStart, s) != base);
    s = sample();
    s.turrets ^= 1u; // spec 019: the turrets' state word
    CHECK(checksum_add(kChecksumStart, s) != base);
    s = sample();
    s.entities = 8; // spec 020: the live entities and their handles
    CHECK(checksum_add(kChecksumStart, s) != base);
    s = sample();
    s.handles ^= 0x10000u;
    CHECK(checksum_add(kChecksumStart, s) != base);
}

NNH_TEST(replay_checksum_depends_on_the_order_of_the_samples) {
    ReplaySample other = sample();
    other.coins = 9;
    CHECK(checksum_add(checksum_add(kChecksumStart, sample()), other) !=
          checksum_add(checksum_add(kChecksumStart, other), sample()));
}

NNH_TEST(replay_run_starts_at_the_first_frame_of_the_first_segment) {
    const ReplayRun run = two_segments();
    CHECK(!run.done());
    CHECK(run.level() == Demo::Level1);
    CHECK(run.frame() == 0);
    CHECK(run.total_frame() == 0);
    CHECK(run.total_frames() == 180);
    CHECK(run.input() == kRight);
    CHECK(run.checksum() == kChecksumStart);
}

NNH_TEST(replay_run_follows_the_script_frame_by_frame) {
    ReplayRun run = two_segments();
    play(run, 2);
    CHECK(run.input() == kJumpRight);
    play(run, 1);
    CHECK(run.input() == kRight);
}

NNH_TEST(replay_run_checksums_every_recorded_sample) {
    ReplayRun run = two_segments();
    play(run, 2);
    CHECK(run.checksum() == checksum_add(checksum_add(kChecksumStart, sample()), sample()));
}

NNH_TEST(replay_run_reports_a_checkpoint_every_60_frames) {
    ReplayRun run = two_segments();
    CHECK(!play(run, 59).checkpoint);
    const ReplayProgress p = play(run, 1);
    CHECK(p.checkpoint);
    CHECK(!p.segment_end);
    CHECK(run.total_frame() == 60);
    CHECK(!play(run, 1).checkpoint);
}

NNH_TEST(replay_run_moves_to_the_next_segment_and_restarts_its_script) {
    ReplayRun run = two_segments();
    const ReplayProgress p = play(run, 120);
    CHECK(p.segment_end);
    CHECK(p.checkpoint);
    CHECK(!p.finished);
    CHECK(run.level() == Demo::Bonus);
    CHECK(run.segment() == 1);
    CHECK(run.frame() == 0);
    CHECK(run.input() == kRight);
}

NNH_TEST(replay_run_finishes_after_the_last_segment) {
    ReplayRun run = two_segments();
    play(run, 120);
    CHECK(!play(run, 59).finished);
    const ReplayProgress p = play(run, 1);
    CHECK(p.finished);
    CHECK(p.segment_end);
    CHECK(run.done());
    CHECK(run.total_frame() == 180);
    CHECK(run.level() == Demo::Bonus);
}

NNH_TEST(replay_run_ignores_frames_recorded_after_it_finished) {
    ReplayRun run = two_segments();
    play(run, 180);
    const std::uint32_t sum = run.checksum();
    const ReplayProgress p = play(run, 5);
    CHECK(!p.checkpoint && !p.finished);
    CHECK(run.checksum() == sum);
    CHECK(run.total_frame() == 180);
}

NNH_TEST(replay_run_measures_each_segment_without_its_first_frame) {
    ReplayRun run = two_segments();
    run.record(sample(), 500); // the frame that switched to the level: not measured
    play(run, 119, 16.7);
    run.record(sample(), 900); // the bonus level's first frame
    play(run, 58, 16.7);
    run.record(sample(), 30); // missed
    CHECK(run.meter(0).frames() == 119);
    CHECK(run.meter(0).ok());
    CHECK(run.meter(1).frames() == 59);
    CHECK(run.meter(1).missed() == 1);
    CHECK(run.meter(1).worst_ms() == 30);
}

NNH_TEST(replay_run_without_segments_is_done) {
    const ReplayRun run;
    CHECK(run.done());
    CHECK(run.total_frames() == 0);
    CHECK(run.input() == PlayerInput{});
}
