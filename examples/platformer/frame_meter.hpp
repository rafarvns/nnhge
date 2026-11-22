#pragma once
// Frame times of a stretch of play, for the in-game 60 Hz verdict (pure, tested in
// tests/platformer/test_frame_meter.cpp). A frame longer than 1.5 slots of 60 Hz (25 ms) missed its slot,
// the same rule as the benchmark's gate (gate_run.hpp).

class FrameMeter {
  public:
    static constexpr double kMissedMs = 1.5 * 1000.0 / 60.0;

    void reset() { *this = {}; }
    /// One frame took `ms` milliseconds.
    void add(double ms);

    int frames() const { return frames_; }
    int missed() const { return missed_; }
    double worst_ms() const { return worst_ms_; }
    double average_ms() const { return frames_ > 0 ? total_ms_ / frames_ : 0; }
    /// At 60 Hz: some frames measured and none missed.
    bool ok() const { return frames_ > 0 && missed_ == 0; }

  private:
    int frames_ = 0;
    int missed_ = 0;
    double total_ms_ = 0;
    double worst_ms_ = 0;
};
