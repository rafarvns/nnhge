#pragma once
// The models example's self-check (spec 026 AC-2/AC-7): a fixed, seeded script of 50 000 operations of the 3D math
// (transforms composed and inverted, quaternions built, multiplied and interpolated, look-at and projections, points
// projected) whose result bits are folded into one checksum. The same golden on every host compiler and on the 3DS.
// Pure: no graphics; shared with tests/models_example/test_self_check.cpp.

#include <cstdint>

namespace models {

class SelfCheck {
public:
    static constexpr int kOps = 50000;
    static constexpr int kCheckpointEvery = 10000;

    /// Runs up to `count` more operations (a frame's share). @return true once all kOps have run.
    bool run(int count);
    int done() const { return step_; }
    std::uint32_t sum() const { return sum_; }
    /// The checksum after checkpoint `k` (1..5), or 0 before it.
    std::uint32_t checkpoint(int k) const { return k >= 1 && k <= 5 ? checkpoints_[k - 1] : 0; }

private:
    void one();
    void mix(std::uint32_t word);
    void mix(float f);
    std::uint32_t hash(std::uint32_t salt) const;
    float value(std::uint32_t salt, float range) const;

    int step_ = 0;
    std::uint32_t sum_ = 2166136261u;
    std::uint32_t checkpoints_[5] = {};
};

/// The golden checksum of the whole script (recorded from the simulator; the console must print the same).
inline constexpr std::uint32_t kSelfCheckGolden = 0x436E15A9u; // stage 1: math only

} // namespace models
