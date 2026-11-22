#pragma once
// Zoomed tile edges (spec 018 FR-9, decision D33). Pure integer code; unit tested in tests/tilemap/test_zoom_edges.cpp.
//
// At zoom z a layer's cell i spans [edge(i), edge(i + 1)) on screen, with
//     edge(i) = origin + round_half_up(i × tile × z)
// computed in 16.16 fixed point (z as zoom_q() below), so tiles never gap or overlap, every step is
// exactly floor(tile × z) or one more, integer zooms give exact multiples, and PC and 3DS agree bit for bit.

#include <cstdint>

namespace nnh::detail {

/// A zoom in 16.16 fixed point (zoom 1 = 65536, zoom in kMinCameraZoom..kMaxCameraZoom). Tile edges are computed from
/// it in integers, so they are the same on every system and every step is exactly floor or ceil of tile × zoom.
inline int zoom_q(float zoom) { return static_cast<int>(zoom * 65536.0f + 0.5f); }

/// The most cells one axis of a screen shows at the smallest zoom (8-pixel tiles at 0.5 on the 400-pixel top screen:
/// 101), with room to spare. Edge tables hold one more entry than cells.
inline constexpr int kMaxZoomCells = 127;

/// edge(i) − origin for cell index `i` ≥ 0 of `tile`-pixel tiles at zoom `zq` (16.16).
inline int zoom_edge(int i, int tile, int zq) {
    return static_cast<int>((static_cast<std::int64_t>(i) * tile * zq + 32768) >> 16);
}

/// floor(tile × zoom) at zoom `zq` (16.16): the smaller of the two step sizes (TileBatch2D::dst_w).
inline int zoomed_size(int tile, int zq) { return static_cast<int>((static_cast<std::int64_t>(tile) * zq) >> 16); }

/// Cells [first, last) of one axis whose spans [origin + zoom_edge(i), origin + zoom_edge(i + 1)) meet [0, screen),
/// clamped to [0, cells).
void zoom_axis_range(int origin, int tile, int zq, int screen, int cells, int& first, int& last);

/// Writes origin + zoom_edge(i) for i = first .. first + count (count + 1 values, count ≤ kMaxZoomCells) to `out`.
void zoom_edges(int origin, int tile, int zq, int first, int count, std::int16_t* out);

} // namespace nnh::detail
