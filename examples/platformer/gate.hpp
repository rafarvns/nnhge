#pragma once
// The benchmark's automatic gate (spec 013 FR-6, spec 012 T3.9): runs the trials of GateRun, logs each one
// and the verdict (`bench: PASS ...` / `bench: FAIL ...`, what a console test script waits for) and shows
// progress and the verdict on screen, so a person reading the console needs no log either. At a camera zoom other
// than 1 (spec 018) the same trials only measure: `bench: zoom Z limits ...` and `bench: zoom Z done`, no verdict.

#include "gate_run.hpp"

class Gate {
  public:
    /// Starts a new run (logs `bench: gate started ...`) at the bench's camera `zoom` (≠ 1: measure only). The bench
    /// draws counts() from the next frame.
    void start(float zoom = 1);
    /// Stops a run before its end (logs it; no verdict).
    void stop();
    bool running() const { return running_; }
    /// A run ended and its verdict is known.
    bool finished() const { return finished_; }
    /// The run measures limits only (a zoom other than 1): no PASS/FAIL.
    bool measure_only() const { return zoom_ != 1; }
    bool passed() const { return run_.passed(); }
    /// Counts of the trial under way.
    GateCounts counts() const { return run_.counts(); }

    /// Feeds the frame time of the frame now starting; logs what ended. Returns the counts to draw.
    GateCounts frame(double ms);

    /// Bottom-screen panel: the trial under way, or the verdict and limits once finished.
    void draw_panel() const;
    /// Top-screen verdict banner (finished runs only), over the benchmark.
    void draw_banner() const;

  private:
    void log_trial() const;
    void log_layer_limit() const;
    void log_finish() const;
    void draw_progress() const;
    void draw_verdict() const;

    GateRun run_;
    float zoom_ = 1;
    bool running_ = false;
    bool finished_ = false;
};
