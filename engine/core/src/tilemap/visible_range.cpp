// Where a tile layer lands on a screen and which cells it shows (see visible_range.hpp).

#include "tilemap/visible_range.hpp"

#include "tilemap/zoom_edges.hpp"

#include <cmath>
#include <cstdint>

namespace nnh::detail {
namespace {

// Beyond this the layer is far off screen (a map is at most 1024 × 64 = 65536 px wide).
constexpr float kFar = 1073741824.0f; // 2^30

/// floor(a / b) for b > 0.
std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    const std::int64_t q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

std::int64_t clamp64(std::int64_t v, std::int64_t lo, std::int64_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/// Cells [first, last) of one axis whose pixels [origin + i·tile, origin + (i+1)·tile) meet [0, screen).
void axis_range(int origin, int tile, int screen, int cells, int& first, int& last) {
    first = static_cast<int>(clamp64(floor_div(-static_cast<std::int64_t>(origin), tile), 0, cells));
    last = static_cast<int>(
        clamp64(floor_div(static_cast<std::int64_t>(screen) - origin - 1, tile) + 1, 0, cells));
}

/// The zoomed view (zoom_q ≠ 65536): parallax around the zoomed view's centre, edges from zoom_edges.
LayerView zoomed_range(const LayerPlacement& p, int zq, int tile_w, int tile_h, int map_w, int map_h) {
    const float z = p.zoom;
    const Vec2 shift = p.camera + (p.screen_size / z) * 0.5f - p.parallax_origin;
    const float ox = std::floor((p.offset.x + shift.x * (1.0f - p.parallax.x) - p.camera.x) * z + 0.5f);
    const float oy = std::floor((p.offset.y + shift.y * (1.0f - p.parallax.y) - p.camera.y) * z + 0.5f);
    LayerView v;
    v.zoom_q = zq;
    if (!(ox > -kFar && ox < kFar && oy > -kFar && oy < kFar)) return v;
    v.origin_x = static_cast<int>(ox);
    v.origin_y = static_cast<int>(oy);
    zoom_axis_range(v.origin_x, tile_w, zq, static_cast<int>(p.screen_size.x), map_w, v.x0, v.x1);
    zoom_axis_range(v.origin_y, tile_h, zq, static_cast<int>(p.screen_size.y), map_h, v.y0, v.y1);
    return v;
}

} // namespace

LayerView visible_range(const LayerPlacement& p, int tile_w, int tile_h, int map_w, int map_h) {
    const int zq = zoom_q(p.zoom);
    if (zq != 65536) return zoomed_range(p, zq, tile_w, tile_h, map_w, map_h);
    const Vec2 shift = p.camera + p.screen_size * 0.5f - p.parallax_origin;
    const float ox = std::floor(p.offset.x + shift.x * (1.0f - p.parallax.x) - p.camera.x + 0.5f);
    const float oy = std::floor(p.offset.y + shift.y * (1.0f - p.parallax.y) - p.camera.y + 0.5f);
    LayerView v;
    // NaN fails both comparisons, so it lands here too.
    if (!(ox > -kFar && ox < kFar && oy > -kFar && oy < kFar)) return v;
    v.origin_x = static_cast<int>(ox);
    v.origin_y = static_cast<int>(oy);
    axis_range(v.origin_x, tile_w, static_cast<int>(p.screen_size.x), map_w, v.x0, v.x1);
    axis_range(v.origin_y, tile_h, static_cast<int>(p.screen_size.y), map_h, v.y0, v.y1);
    return v;
}

} // namespace nnh::detail
