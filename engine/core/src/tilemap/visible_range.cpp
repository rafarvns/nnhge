// Where a tile layer lands on a screen and which cells it shows (see visible_range.hpp).

#include "tilemap/visible_range.hpp"

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

} // namespace

LayerView visible_range(const LayerPlacement& p, int tile_w, int tile_h, int map_w, int map_h) {
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
