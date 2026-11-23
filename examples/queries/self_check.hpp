#pragma once
// The queries example's self-check (spec 019 AC-2, AC-6): a fixed script of 4096 rays and 512 box casts on the
// engine's real queries, against a fixed grid with solid and one-way cells (walled and open) and 64 boxes on layers
// (some without area, some on no layers), with filters (masks, ignore indices, hits from inside, ignore one-way)
// from the engine-free integer hash below. Each ray runs raycast on the grid, the boxes and both, raycast_all on
// each, cells_crossed and line_of_sight; each cast box_cast and box_cast_all. An FNV-1a checksum takes the bits of
// every result (fractions, distances, points, normals, indices, cells, kinds, counts, order). It must be the same
// on every compiler and on the 3DS. Pure code (no drawing, no input), so the unit tests run it
// (tests/queries_example/test_self_check.cpp).

#include <cstddef>
#include <cstdint>
#include <nnh/collision.hpp>

/// The checksum the simulator recorded for the whole script. A different value on any system is a determinism
/// hole (or a change of the script or of the collision module: then record the new value from pc-debug, and the
/// checkpoint lines of examples/queries/tests/check.nnhs).
inline constexpr std::uint32_t kQueriesCheckGolden = 0x4ACAA177u; // pc-debug, WinLibs GCC 16

class QueriesSelfCheck {
  public:
    static constexpr int kRays = 4096;
    static constexpr int kCasts = 512;
    static constexpr int kCheckpointEvery = 1024; ///< rays
    static constexpr int kGridW = 24, kGridH = 16, kBoxes = 64;

    /// What the script reached (so its tests can tell it isn't vacuous).
    struct Stats {
        int grid_hits = 0, box_hits = 0, inside_hits = 0, one_way_hits = 0, blocked_sights = 0, cast_hits = 0;
    };

    QueriesSelfCheck();

    /// Runs the next ray (or, after the rays, the next cast). @return true when that was ray 1024, 2048, ...
    bool step();
    bool done() const { return casts_ >= kCasts; }
    int rays() const { return rays_; }
    int casts() const { return casts_; }
    std::uint32_t checksum() const { return sum_; }
    const Stats& stats() const { return stats_; }

  private:
    void ray();
    void cast();
    std::uint32_t next();
    nnh::Vec2 point();
    nnh::collision::QueryFilter filter();
    void mix(std::uint32_t word);
    void mix_float(float value);
    void mix_hit(const nnh::collision::RayHit& hit);

    nnh::collision::Cell cells_[kGridW * kGridH] = {};
    nnh::collision::Grid open_{};   ///< outside Empty: clipped
    nnh::collision::Grid walled_{}; ///< outside Solid
    nnh::Rect boxes_[kBoxes] = {};
    nnh::collision::Layers layers_[kBoxes] = {};
    std::uint32_t state_ = 0x13579BDFu;
    std::uint32_t sum_ = 2166136261u;
    int rays_ = 0;
    int casts_ = 0;
    Stats stats_;
};

/// "queries: <rays> sum XXXXXXXX" (a checkpoint log line).
void format_queries_checkpoint(char* out, std::size_t size, int rays, std::uint32_t sum);
/// "queries: PASS XXXXXXXX", or "queries: FAIL sum XXXXXXXX expected XXXXXXXX".
void format_queries_verdict(char* out, std::size_t size, std::uint32_t sum);
