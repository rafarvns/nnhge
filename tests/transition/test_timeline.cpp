// The frame timeline (spec 023 FR-2, FR-3; plan 023 "The frame timeline"): whole frames per phase, coverage per
// frame from the easing of an exact ratio, ratchets, forced ends, at least one covered frame.

#include <cstring>

#include "test.hpp"
#include "transition/transition_internal.hpp"

using nnh::Duration;
using nnh::Ease;
using nnh::Frames;
using nnh::FrameTime;
using nnh::detail::Timeline;
using nnh::detail::TimelineEnd;
using nnh::transition::Phase;

namespace {

/// Plays a whole transition with no loading step; returns its frame count and, if asked, the coverage per frame.
int play(int out, int hold, int in, nnh::Easing oe = {}, nnh::Easing ie = {}, float* coverage = nullptr,
         Phase* phases = nullptr) {
    Timeline t;
    nnh::detail::timeline_start(t, out, hold, in);
    for (int f = 0; f < 5000; ++f) {
        nnh::detail::timeline_step(t, oe, ie);
        if (coverage != nullptr) coverage[f] = t.coverage;
        if (phases != nullptr) phases[f] = t.phase;
        if (nnh::detail::timeline_end(t, true, false) == TimelineEnd::Done) return f + 1;
    }
    return -1;
}

bool bits_equal(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

} // namespace

NNH_TEST(durations_round_to_whole_frames_half_up) {
    CHECK(nnh::detail::frames_of(Frames{24}) == 24);
    CHECK(nnh::detail::frames_of(Duration::from_ms(500)) == 30);
    CHECK(nnh::detail::frames_of(FrameTime::from_ticks(2047)) == 0);
    CHECK(nnh::detail::frames_of(FrameTime::from_ticks(2048)) == 1);
    CHECK(nnh::detail::frames_of(FrameTime::from_ticks(6143)) == 1);
    CHECK(nnh::detail::frames_of(FrameTime::from_ticks(6144)) == 2);
    CHECK(nnh::detail::frames_of(Duration::from_ms(10)) == 1); // 0.6 frames
    CHECK(nnh::detail::frames_of(Duration::from_seconds(10)) == 600);
    CHECK(nnh::detail::frames_of(Duration::from_seconds(11)) == 600); // clamped (requests assert first)
    CHECK(nnh::detail::frames_of(Frames{-1}) == 0);                   // invalid
}

NNH_TEST(a_transition_takes_out_plus_covered_plus_in_frames) {
    const int cases[][3] = {{24, 0, 24}, {1, 0, 1}, {10, 5, 3}, {0, 0, 20}, {20, 0, 0}, {0, 7, 0}, {3, 1, 0},
                            {600, 600, 600}};
    for (const auto& c : cases) {
        const int covered = c[1] > 0 ? c[1] : 1;
        CHECK(play(c[0], c[1], c[2]) == c[0] + covered + c[2]);
    }
    CHECK(play(0, 0, 0) == 1); // all zero: the change plus one covered frame
}

NNH_TEST(phases_follow_each_other_frame_by_frame) {
    Phase phases[16];
    CHECK(play(3, 2, 2, {}, {}, nullptr, phases) == 7);
    const Phase expected[] = {Phase::Out, Phase::Out, Phase::Out, Phase::Covered, Phase::Covered, Phase::In,
                              Phase::In};
    bool ok = true;
    for (int i = 0; i < 7; ++i) ok = ok && phases[i] == expected[i];
    CHECK(ok);
}

NNH_TEST(linear_coverage_is_k_over_n_and_exact_at_the_ends) {
    float c[16];
    CHECK(play(4, 0, 4, {}, {}, c) == 9);
    const float expected[] = {0.25f, 0.5f, 0.75f, 1.0f, 1.0f, 0.75f, 0.5f, 0.25f, 0.0f};
    bool ok = true;
    for (int i = 0; i < 9; ++i) ok = ok && bits_equal(c[i], expected[i]);
    CHECK(ok);
}

NNH_TEST(quad_in_coverage_is_the_square_of_the_frame_ratio) {
    float c[16];
    CHECK(play(4, 0, 4, Ease::QuadIn, Ease::QuadIn, c) == 9);
    const float expected[] = {0.0625f, 0.25f, 0.5625f, 1.0f, 1.0f, 0.5625f, 0.25f, 0.0625f, 0.0f};
    bool ok = true;
    for (int i = 0; i < 9; ++i) ok = ok && bits_equal(c[i], expected[i]);
    CHECK(ok);
}

NNH_TEST(the_in_phase_plays_the_out_phase_backwards) {
    const Ease curves[] = {Ease::Linear, Ease::CubicInOut, Ease::SineIn, Ease::QuadOut};
    for (Ease e : curves) {
        float c[64];
        CHECK(play(24, 0, 24, e, e, c) == 49);
        bool ok = true;
        for (int k = 1; k < 24; ++k) ok = ok && bits_equal(c[24 + k], c[24 - k - 1]); // in k == out 24 − k
        CHECK(ok);
    }
}

NNH_TEST(overshooting_curves_keep_coverage_monotonic_and_inside_0_1) {
    const Ease curves[] = {Ease::BackIn, Ease::BackOut, Ease::BackInOut, Ease::ElasticIn, Ease::ElasticOut,
                           Ease::ElasticInOut, Ease::BounceIn, Ease::BounceOut, Ease::BounceInOut};
    for (Ease e : curves) {
        float c[128];
        const int n = play(40, 0, 40, e, e, c);
        CHECK(n == 81);
        bool ok = true;
        for (int i = 0; i < n; ++i) ok = ok && c[i] >= 0.0f && c[i] <= 1.0f;
        for (int i = 1; i < 40; ++i) ok = ok && c[i] >= c[i - 1];
        for (int i = 41; i < 81; ++i) ok = ok && c[i] <= c[i - 1];
        CHECK(ok && c[39] == 1.0f && c[80] == 0.0f);
    }
}

NNH_TEST(the_covered_phase_waits_for_the_loading_step_and_the_hold) {
    Timeline t;
    nnh::detail::timeline_start(t, 1, 3, 2);
    nnh::detail::timeline_step(t, {}, {});
    CHECK(nnh::detail::timeline_end(t, true, false) == TimelineEnd::OutEnded);
    for (int f = 1; f <= 10; ++f) { // loading not done: stays covered past the hold
        nnh::detail::timeline_step(t, {}, {});
        CHECK(t.phase == Phase::Covered && t.covered == static_cast<std::uint32_t>(f));
        CHECK(nnh::detail::timeline_end(t, false, false) == TimelineEnd::Continue);
    }
    nnh::detail::timeline_step(t, {}, {});
    CHECK(nnh::detail::timeline_end(t, true, false) == TimelineEnd::Continue);
    CHECK(t.phase == Phase::In);
}

NNH_TEST(an_in_phase_over_nothing_is_skipped) {
    Timeline t;
    nnh::detail::timeline_start(t, 0, 0, 30);
    nnh::detail::timeline_step(t, {}, {});
    CHECK(nnh::detail::timeline_end(t, true, true) == TimelineEnd::Done);
}

NNH_TEST(phase_progress_counts_frames_of_the_phase) {
    Timeline t;
    nnh::detail::timeline_start(t, 4, 2, 5);
    nnh::detail::timeline_step(t, {}, {});
    CHECK(nnh::detail::timeline_phase_progress(t) == 0.25f);
    for (int i = 0; i < 3; ++i) nnh::detail::timeline_step(t, {}, {});
    nnh::detail::timeline_end(t, true, false);
    nnh::detail::timeline_step(t, {}, {});
    CHECK(nnh::detail::timeline_phase_progress(t) == 0.5f);
    nnh::detail::timeline_end(t, true, false);
    nnh::detail::timeline_step(t, {}, {});
    CHECK(nnh::detail::timeline_phase_progress(t) == 1.0f);
    nnh::detail::timeline_end(t, true, false);
    nnh::detail::timeline_step(t, {}, {});
    CHECK(t.phase == Phase::In && nnh::detail::timeline_phase_progress(t) == 0.2f);
    Timeline n;
    nnh::detail::timeline_start(n, 0, 0, 0);
    nnh::detail::timeline_step(n, {}, {});
    CHECK(nnh::detail::timeline_phase_progress(n) == 1.0f); // no hold
}

NNH_TEST(coverage_clamps_nan_and_out_of_range_values) {
    CHECK(nnh::detail::clamp_unit(0.0f / 0.0f) == 0.0f);
    CHECK(nnh::detail::clamp_unit(-0.5f) == 0.0f);
    CHECK(nnh::detail::clamp_unit(1.5f) == 1.0f);
    CHECK(nnh::detail::clamp_unit(0.25f) == 0.25f);
    float c[8];
    CHECK(play(3, 0, 0, +[](float) { return 0.0f / 0.0f; }, {}, c) == 4); // a game's curve returning NaN
    CHECK(c[0] == 0.0f && c[1] == 0.0f && c[2] == 1.0f);
}
