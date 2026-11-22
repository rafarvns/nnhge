#pragma once
// Animated tiles (spec 012 stage 6, plan "Animated tiles"): the current frame of each animation and the table the
// tile expansion reads. Pure; unit tested in tests/tilemap/test_tile_animation.cpp.
//
// A map's animations are sorted by tile id, each owning a run of frames (tile id + ms) that loops over its total
// duration. At submit, a batch resolves the current frame of each animation of its tileset once (≤ 256 small loops)
// into a stack table of sources; a cell whose tile flag says "animated" reads its source from that table (binary
// search over ≤ 256 entries, only for animated tiles). No per-cell state; collision never looks at animations.

#include <cstdint>

#include "tilemap/map_format.hpp"
#include "tilemap/map_layout.hpp"
#include "tilemap/tile_expand.hpp"

namespace nnh::detail {

/// The default clock: milliseconds at frame `frame` of a 60 Hz loop (frame · 1000 / 60, rounded down).
constexpr std::uint64_t default_animation_ms(std::uint64_t frame) { return frame * 1000u / 60u; }

/// Index of the frame shown at `time_ms` (0 at time 0; a frame of d ms is shown during [start, start + d)); the
/// frames loop over their total duration. `count` ≥ 1 and every ms ≥ 1 (a validated map).
int animation_frame_index(const mapfmt::FrameEntry* frames, int count, std::uint64_t time_ms);

/// Index of the animation of `tile_id` in `animations` (`count` entries sorted by tile id), or -1.
int find_animation(const mapfmt::AnimationEntry* animations, int count, int tile_id);

struct AnimationRange {
    int first = 0; ///< index of the first animation of the range
    int count = 0;
};

/// The animations whose tile is in [first_id, first_id + tile_count) (one tileset): a run of the sorted table.
AnimationRange animations_in(const mapfmt::AnimationEntry* animations, int count, int first_id, int tile_count);

/// Prepares `in` (a pass over tileset `tileset` of map `v`) for animated tiles at `time_ms`: resolves the current
/// source of each animation of that tileset into `table` (kMaxAnimations entries) and points `in` at it. Leaves
/// `in` without animations when the tileset has none (the expansion then never looks at the flags).
void prepare_animations(const MapView& v, int tileset, std::uint64_t time_ms, mapfmt::TileSource* table,
                        ExpandInput& in);

} // namespace nnh::detail
