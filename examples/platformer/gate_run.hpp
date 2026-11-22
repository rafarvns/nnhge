#pragma once
// The automatic gate's bookkeeping (spec 013 FR-6, spec 012 T3.9), with no engine calls so it is unit-tested
// (tests/platformer/test_gate_run.cpp). Feed it one frame time per frame; it says which counts to draw and
// when a trial, a limit search or the whole gate ends.
//
// Trials: set the counts, skip `skip_frames` (the change and anything slow around it, like the log line of
// the previous trial), then measure `frames`; a trial passes when no measured frame missed its 60 Hz slot.
// First the target (4 layers + 300 sprites, 600 frames), then the limits, 300 frames each: layers one by one
// with the target's sprites, then sprites by `sprite_step` with the target's layers. A search goes up from
// the target while trials pass (the limit is the last count that passed) or, when the target failed, down
// until one passes.

#include <cstddef>
#include <nnh/math.hpp>

struct GateCounts {
    int layers = 0;
    int sprites = 0;
    bool operator==(const GateCounts&) const = default;
};

struct GateConfig {
    GateCounts target{4, 300};
    int min_layers = 1;
    int max_layers = 12;
    int max_sprites = 2000;
    int sprite_step = 100;
    int skip_frames = 30;
    int target_frames = 600;
    int search_frames = 300;
    double missed_ms = 1.5 * 1000.0 / 60.0; ///< a frame slower than this missed its 60 Hz slot
};

/// One trial's measurement.
struct GateTrial {
    GateCounts counts;
    int frames = 0; ///< measured so far (skipped frames not counted)
    int missed = 0;
    double total_ms = 0;
    double worst_ms = 0;
    double average_ms() const { return frames > 0 ? total_ms / frames : 0; }
    bool ok() const { return frames > 0 && missed == 0; }
};

enum class GatePhase { Target, Layers, Sprites, Done };

/// What a frame finished. Every value but None means a trial ended (read it with last()).
enum class GateStep {
    None,
    Trial,      ///< a trial ended; the search goes on
    LayerLimit, ///< a trial ended and settled layer_limit(); the sprite search starts
    Finished,   ///< a trial ended and settled sprite_limit(); the gate is over
};

class GateRun {
  public:
    /// Starts with the target trial. Expects min_layers < target.layers < max_layers and
    /// sprite_step <= target.sprites <= max_sprites - sprite_step.
    explicit GateRun(const GateConfig& config = {});

    GatePhase phase() const { return phase_; }
    bool done() const { return phase_ == GatePhase::Done; }
    /// Counts to draw this frame (the current trial's).
    GateCounts counts() const { return current_.counts; }
    /// Frames of the current trial so far (skipped ones included) and its full length.
    int trial_frame() const { return position_; }
    int trial_length() const;
    /// Trials finished so far (the target included).
    int trials() const { return trials_; }

    /// One frame took `ms`. Counts the frame into the current trial and moves on when the trial ends.
    GateStep frame(double ms);

    const GateTrial& last() const { return last_; }
    const GateTrial& target() const { return target_; }
    /// The gate's verdict: the target trial passed.
    bool passed() const { return target_.ok(); }
    /// Most layers that passed with the target's sprites; 0 = none did. Valid from GateStep::LayerLimit.
    int layer_limit() const { return layer_limit_; }
    /// Most sprites that passed with the target's layers; -1 = none did (not even 0). Valid once done().
    int sprite_limit() const { return sprite_limit_; }

  private:
    void start(GateCounts counts);
    GateStep next_layer_trial();
    GateStep next_sprite_trial();

    GateConfig config_;
    GatePhase phase_ = GatePhase::Target;
    GateTrial current_;
    GateTrial last_;
    GateTrial target_;
    int position_ = 0;
    int trials_ = 0;
    int layer_limit_ = 0;
    int sprite_limit_ = -1;
};

// ---- The bench's zoom (spec 018 T7.2): B cycles it; A at a zoom other than 1 runs the same trials to measure the
// limits only (no pass threshold: zooming out is the game's cost choice).

/// The zoom after `zoom`: 1 → 0.5 → 2 → 1.
float next_bench_zoom(float zoom);
/// The largest camera position the bench's wandering camera reaches at `zoom` (from 16, 16): (288, 96) at zoom 1, as
/// before spec 018 (its screenshots stay the same); else the most that keeps the view on the 64 × 40-cell map.
nnh::Vec2 bench_camera_max(float zoom);
/// "bench: zoom 0.50 limits L layers (S sprites), N sprites (M layers)" (a sprite limit of -1 is "none").
void format_zoom_limits(char* out, std::size_t size, float zoom, int layer_limit, int sprite_limit,
                        GateCounts target);
/// "bench: zoom 0.50 done".
void format_zoom_done(char* out, std::size_t size, float zoom);
