// The platformer's automatic gate bookkeeping (examples/platformer/gate_run.hpp, spec 013 FR-6). Example
// code, but pure, so it is tested here like core logic.

#include "platformer/gate_run.hpp"
#include "test.hpp"

namespace {

constexpr double kGood = 16.0;
constexpr double kSlow = 40.0;

/// Feeds frames until the current trial ends: all good, or with the last measured frame slow.
GateStep run_trial(GateRun& gate, bool pass) {
    const int length = gate.trial_length();
    for (int i = 1; i < length; ++i) {
        if (gate.frame(kGood) != GateStep::None) return GateStep::None; // ended early: wrong
    }
    return gate.frame(pass ? kGood : kSlow);
}

bool counts_are(const GateRun& gate, int layers, int sprites) {
    return gate.counts() == GateCounts{layers, sprites};
}

} // namespace

NNH_TEST(gate_starts_with_the_target_trial) {
    GateRun gate;
    CHECK(gate.phase() == GatePhase::Target);
    CHECK(counts_are(gate, 4, 300));
    CHECK(gate.trial_length() == 30 + 600);
    CHECK(gate.trial_frame() == 0);
    CHECK(!gate.done());
}

NNH_TEST(gate_trial_ends_after_its_skipped_and_measured_frames) {
    GateRun gate;
    for (int i = 0; i < 629; ++i)
        CHECK(gate.frame(kGood) == GateStep::None);
    CHECK(gate.trial_frame() == 629);
    CHECK(gate.frame(kGood) == GateStep::Trial);
    CHECK(gate.last().frames == 600);
    CHECK(gate.last().counts == (GateCounts{4, 300}));
    CHECK(gate.trials() == 1);
}

NNH_TEST(gate_does_not_measure_the_skipped_frames) {
    GateRun gate;
    for (int i = 0; i < 30; ++i)
        gate.frame(500.0);
    for (int i = 0; i < 600; ++i)
        gate.frame(kGood);
    CHECK(gate.last().ok());
    CHECK(gate.last().worst_ms == kGood);
}

NNH_TEST(gate_trial_with_a_missed_frame_fails) {
    GateRun gate;
    CHECK(run_trial(gate, false) == GateStep::Trial);
    CHECK(gate.last().missed == 1);
    CHECK(!gate.last().ok());
    CHECK(gate.last().worst_ms == kSlow);
}

NNH_TEST(gate_trial_records_average_and_worst) {
    GateRun gate;
    for (int i = 0; i < 30; ++i)
        gate.frame(kGood);
    for (int i = 0; i < 600; ++i)
        gate.frame(i % 2 == 0 ? 15.0 : 17.0);
    CHECK(gate.last().average_ms() == 16.0);
    CHECK(gate.last().worst_ms == 17.0);
}

NNH_TEST(gate_frame_at_exactly_the_threshold_is_not_missed) {
    GateRun gate({.missed_ms = 25.0});
    for (int i = 0; i < 630; ++i)
        gate.frame(25.0);
    CHECK(gate.last().ok());
}

NNH_TEST(gate_passing_target_searches_layers_upwards) {
    GateRun gate;
    run_trial(gate, true);
    CHECK(gate.phase() == GatePhase::Layers);
    CHECK(counts_are(gate, 5, 300));
    CHECK(gate.trial_length() == 30 + 300);
    CHECK(gate.trial_frame() == 0);
}

NNH_TEST(gate_layer_limit_is_the_last_count_that_passed) {
    GateRun gate;
    run_trial(gate, true);                                 // target
    CHECK(run_trial(gate, true) == GateStep::Trial);       // 5
    CHECK(run_trial(gate, true) == GateStep::Trial);       // 6
    CHECK(run_trial(gate, false) == GateStep::LayerLimit); // 7
    CHECK(gate.last().counts == (GateCounts{7, 300}));
    CHECK(gate.layer_limit() == 6);
    CHECK(gate.phase() == GatePhase::Sprites);
    CHECK(counts_are(gate, 4, 400));
}

NNH_TEST(gate_layer_limit_is_the_target_when_one_more_fails) {
    GateRun gate;
    run_trial(gate, true);
    CHECK(run_trial(gate, false) == GateStep::LayerLimit);
    CHECK(gate.layer_limit() == 4);
}

NNH_TEST(gate_layer_search_stops_at_the_most_layers) {
    GateRun gate;
    run_trial(gate, true);
    for (int layers = 5; layers < 12; ++layers)
        CHECK(run_trial(gate, true) == GateStep::Trial);
    CHECK(counts_are(gate, 12, 300));
    CHECK(run_trial(gate, true) == GateStep::LayerLimit);
    CHECK(gate.layer_limit() == 12);
}

NNH_TEST(gate_sprite_limit_is_the_last_count_that_passed) {
    GateRun gate;
    run_trial(gate, true);
    run_trial(gate, false);                          // 5 layers: limit 4
    CHECK(run_trial(gate, true) == GateStep::Trial); // 400
    CHECK(counts_are(gate, 4, 500));
    CHECK(run_trial(gate, true) == GateStep::Trial);     // 500
    CHECK(run_trial(gate, false) == GateStep::Finished); // 600
    CHECK(gate.sprite_limit() == 500);
    CHECK(gate.done());
    CHECK(gate.passed());
    CHECK(gate.trials() == 5);
}

NNH_TEST(gate_sprite_search_stops_at_the_most_sprites) {
    GateRun gate;
    run_trial(gate, true);
    run_trial(gate, false);
    for (int sprites = 400; sprites < 2000; sprites += 100)
        CHECK(run_trial(gate, true) == GateStep::Trial);
    CHECK(counts_are(gate, 4, 2000));
    CHECK(run_trial(gate, true) == GateStep::Finished);
    CHECK(gate.sprite_limit() == 2000);
}

NNH_TEST(gate_failing_target_searches_downwards_until_a_trial_passes) {
    GateRun gate;
    run_trial(gate, false);
    CHECK(!gate.passed());
    CHECK(counts_are(gate, 3, 300));
    CHECK(run_trial(gate, false) == GateStep::Trial); // 3
    CHECK(counts_are(gate, 2, 300));
    CHECK(run_trial(gate, true) == GateStep::LayerLimit); // 2
    CHECK(gate.layer_limit() == 2);
    CHECK(counts_are(gate, 4, 200));
    CHECK(run_trial(gate, true) == GateStep::Finished); // 200
    CHECK(gate.sprite_limit() == 200);
    CHECK(!gate.passed());
}

NNH_TEST(gate_failing_everything_gives_no_limits) {
    GateRun gate;
    run_trial(gate, false);
    CHECK(run_trial(gate, false) == GateStep::Trial);      // 3
    CHECK(run_trial(gate, false) == GateStep::Trial);      // 2
    CHECK(run_trial(gate, false) == GateStep::LayerLimit); // 1: the least layers
    CHECK(gate.layer_limit() == 0);
    CHECK(run_trial(gate, false) == GateStep::Trial); // 200
    CHECK(run_trial(gate, false) == GateStep::Trial); // 100
    CHECK(counts_are(gate, 4, 0));
    CHECK(run_trial(gate, false) == GateStep::Finished); // 0
    CHECK(gate.sprite_limit() == -1);
    CHECK(gate.done());
}

NNH_TEST(gate_verdict_is_the_target_trial_only) {
    GateRun gate;
    run_trial(gate, true);
    run_trial(gate, false);
    run_trial(gate, false);
    CHECK(gate.done());
    CHECK(gate.passed());
    CHECK(gate.target().counts == (GateCounts{4, 300}));
    CHECK(gate.target().frames == 600);
}

NNH_TEST(gate_ignores_frames_once_done) {
    GateRun gate;
    run_trial(gate, true);
    run_trial(gate, false);
    run_trial(gate, false);
    CHECK(gate.frame(kSlow) == GateStep::None);
    CHECK(gate.done());
    CHECK(gate.trials() == 3);
    CHECK(gate.passed());
}
