// The frame sequences of FR-4 as pure tables (T2.1): every mode and direction, written out frame by frame.

#include "animation/sprite_format.hpp"
#include "animation/sprite_sequence.hpp"
#include "test.hpp"

using namespace nnh::detail;
using namespace nnh::detail::spritefmt;

namespace {

/// The clip indices of the first `count` positions (cycles repeating).
bool shows(int frames, std::uint8_t mode, std::uint8_t direction, const int* want, int count) {
    const int length = sprite_cycle_length(frames, mode);
    for (int i = 0; i < count; ++i) {
        if (sprite_clip_index(frames, mode, direction, i % length) != want[i]) return false;
    }
    return true;
}

/// Positions shown by a finite clip: (end_cycle · L + end_step + 1).
int shown(int frames, std::uint8_t mode, int count) {
    const SpriteSequenceEnd end = sprite_sequence_end(frames, mode, count);
    return end.end_cycle * sprite_cycle_length(frames, mode) + end.end_step + 1;
}

} // namespace

NNH_TEST(cycle_lengths_follow_the_mode) {
    CHECK(sprite_cycle_length(4, kLoop) == 4);
    CHECK(sprite_cycle_length(4, kOnce) == 4);
    CHECK(sprite_cycle_length(4, kPingPong) == 6);
    CHECK(sprite_cycle_length(2, kPingPong) == 2);
    CHECK(sprite_cycle_length(1, kPingPong) == 1);
    CHECK(sprite_cycle_length(1, kLoop) == 1);
}

NNH_TEST(loop_forward_shows_0_1_2_3_and_wraps) {
    const int want[] = {0, 1, 2, 3, 0, 1, 2, 3};
    CHECK(shows(4, kLoop, kForward, want, 8));
}

NNH_TEST(reverse_starts_from_the_last_frame) {
    const int want[] = {3, 2, 1, 0, 3, 2};
    CHECK(shows(4, kLoop, kReverse, want, 6));
}

NNH_TEST(ping_pong_never_shows_a_turning_frame_twice) {
    const int want[] = {0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1, 0};
    CHECK(shows(4, kPingPong, kForward, want, 13));
}

NNH_TEST(ping_pong_reverse_shows_3_2_1_0_1_2) {
    const int want[] = {3, 2, 1, 0, 1, 2, 3, 2};
    CHECK(shows(4, kPingPong, kReverse, want, 8));
}

NNH_TEST(two_frame_ping_pong_alternates) {
    const int want[] = {0, 1, 0, 1, 0};
    CHECK(shows(2, kPingPong, kForward, want, 5));
}

NNH_TEST(one_frame_clips_show_their_frame) {
    CHECK(sprite_clip_index(1, kPingPong, kForward, 0) == 0);
    CHECK(sprite_clip_index(1, kLoop, kReverse, 0) == 0);
}

NNH_TEST(loops_and_endless_ping_pong_never_end) {
    CHECK(!sprite_sequence_end(4, kLoop, 0).finite);
    CHECK(!sprite_sequence_end(4, kPingPong, 0).finite);
}

NNH_TEST(once_ends_on_the_last_frame_of_the_first_cycle) {
    const SpriteSequenceEnd end = sprite_sequence_end(4, kOnce, 1);
    CHECK(end.finite && end.end_step == 3 && end.end_cycle == 0);
}

NNH_TEST(repeat_three_shows_twelve_positions) {
    const SpriteSequenceEnd end = sprite_sequence_end(4, kRepeat, 3);
    CHECK(end.finite && end.end_step == 3 && end.end_cycle == 2);
    CHECK(shown(4, kRepeat, 3) == 12);
}

NNH_TEST(ping_pong_two_passes_ends_on_the_first_frame) {
    // 0 1 2 3 2 1 0: one round trip, holds 0.
    const SpriteSequenceEnd end = sprite_sequence_end(4, kPingPong, 2);
    CHECK(end.finite && end.end_cycle == 1 && end.end_step == 0);
    CHECK(shown(4, kPingPong, 2) == 7);
    CHECK(sprite_clip_index(4, kPingPong, kForward, end.end_step) == 0);
}

NNH_TEST(ping_pong_odd_passes_end_on_the_far_frame) {
    // Aseprite's repeat 3: 0 1 2 3 2 1 0 1 2 3, holds 3.
    const SpriteSequenceEnd end = sprite_sequence_end(4, kPingPong, 3);
    CHECK(shown(4, kPingPong, 3) == 10);
    CHECK(sprite_clip_index(4, kPingPong, kForward, end.end_step) == 3);
    CHECK(sprite_clip_index(4, kPingPong, kReverse, end.end_step) == 0);
}

NNH_TEST(ping_pong_one_pass_ends_after_the_first_half) {
    const SpriteSequenceEnd end = sprite_sequence_end(4, kPingPong, 1);
    CHECK(shown(4, kPingPong, 1) == 4 && end.end_cycle == 0 && end.end_step == 3);
}

NNH_TEST(one_frame_finite_clips_count_one_position_per_play_or_pass) {
    CHECK(shown(1, kOnce, 1) == 1);
    CHECK(shown(1, kRepeat, 5) == 5);
    CHECK(shown(1, kPingPong, 4) == 4);
}

NNH_TEST(two_frame_ping_pong_passes) {
    CHECK(shown(2, kPingPong, 2) == 3); // 0 1 0
    CHECK(shown(2, kPingPong, 3) == 4); // 0 1 0 1
}

NNH_TEST(the_largest_repeat_fits_the_cycle_counter) {
    const SpriteSequenceEnd end = sprite_sequence_end(256, kRepeat, kMaxRepeat);
    CHECK(end.end_cycle == kMaxRepeat - 1 && end.end_step == 255);
    const SpriteSequenceEnd pp = sprite_sequence_end(256, kPingPong, kMaxRepeat);
    CHECK(pp.end_cycle == kMaxRepeat / 2);
}
