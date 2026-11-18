#pragma once
// A linear volume ramp over time (music fades, spec 008 FR-3). Pure; tested in test_music_track.cpp.

namespace nnh::detail {

struct Fade {
    float from = 1.0f;
    float to = 1.0f;
    double seconds = 0;
    double elapsed = 0;

    /// From `start` to `target` over `duration` seconds (<= 0: at `target` at once).
    static Fade between(float start, float target, double duration) { return Fade{start, target, duration, 0}; }

    float value() const {
        if (seconds <= 0 || elapsed >= seconds) return to;
        return from + (to - from) * static_cast<float>(elapsed / seconds);
    }
    void advance(double dt) { elapsed += dt; }
    bool done() const { return seconds <= 0 || elapsed >= seconds; }
};

} // namespace nnh::detail
