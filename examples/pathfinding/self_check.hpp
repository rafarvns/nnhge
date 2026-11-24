#pragma once
// The pathfinding self-check (spec 025 AC-3, AC-5): a fixed seeded script over four maps — M1 rooms(64, 64, 1), M2
// maze(48, 48, 2), M3 swamp(32, 24, 3), M4 M1's cells at agent size 2 — with 512 searches (every diagonal mode, limits
// 0/50/200, partial on and off, heuristic scales 16/24/64, one-shot or slices of 1/7/64/256), door edits every 32
// searches (a 3 x 3 fill, a set_cell, a sync of a region from a callback grid), a sliced search with an edit in its
// middle every 64 (stale), 64 smoothings and 16 fields with 1-8 goals (some capped, half in slices of 37) with next()
// asked for 64 cells each. An FNV-1a checksum takes every result's bits. Pure (no frame, no drawing): the example runs
// it 8 searches per frame and nnh_tests runs it whole on the fake platform; the golden is the same everywhere.

#include <cstdint>

#include "check_maps.hpp"
#include "nnh/pathfinding.hpp"

namespace pathfinding_demo {

inline constexpr int kCheckSearches = 512;
inline constexpr int kCheckpointEvery = 64;
inline constexpr int kCheckpoints = kCheckSearches / kCheckpointEvery;

/// Recorded from pc-debug; equal in pc-release, on every host compiler and on the 3DS.
extern const std::uint32_t kPathfindingCheckGolden;
extern const std::uint32_t kPathfindingCheckpoints[kCheckpoints];

class SelfCheck {
public:
    /// Builds the maps, a 64 x 64 searcher and a double-buffered 64 x 64 field (load time: allocates). False on no
    /// memory.
    bool init();
    /// Runs up to `n` more searches with their edits, smoothings and fields; returns how many ran.
    int run(int n);
    bool done() const { return done_ >= kCheckSearches; }
    int searches() const { return done_; }
    std::uint32_t checksum() const { return sum_; }
    /// The checksum after search 64 (k + 1) (valid once that many ran).
    std::uint32_t checkpoint(int k) const { return checkpoints_[k]; }

private:
    struct Level {
        nnh::PathMap map;
        std::uint8_t* bytes = nullptr; ///< the cells it was synced from (doors toggle them)
        ByteGrid source;
        int width = 0;
        int height = 0;
    };

    void search(int i);
    void door(int i, Level& level);
    void stale_search(int i, Level& level, nnh::CellPos from, nnh::CellPos to);
    void field(int i, Level& level);
    void smooth(Level& level, int count);
    void mix(std::uint32_t v);

    Level levels_[4];
    std::uint8_t m1_[64 * 64] = {};
    std::uint8_t m2_[48 * 48] = {};
    std::uint8_t m3_[32 * 24] = {};
    std::uint8_t m4_[64 * 64] = {};
    int scratch_[48 * 48 / 4 + 1] = {};
    nnh::PathSearcher searcher_;
    nnh::DistanceField field_;
    nnh::CellPos cells_[64 * 64] = {};
    nnh::CellPos waypoints_[64 * 64] = {};
    int done_ = 0;
    std::uint32_t sum_ = 2166136261u;
    std::uint32_t checkpoints_[kCheckpoints] = {};
};

/// The whole script in one call (tests); fills `checkpoints` (kCheckpoints values) when not null. 0 on no memory.
std::uint32_t run_self_check(std::uint32_t* checkpoints = nullptr);

} // namespace pathfinding_demo
