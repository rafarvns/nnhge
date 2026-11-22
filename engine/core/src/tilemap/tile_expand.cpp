// Visible cells of one (map layer, tileset) → tile quads (see tile_expand.hpp).

#include "tilemap/tile_expand.hpp"

#include "tilemap/tile_animation.hpp"

namespace nnh::detail {

ExpandResult expand_tiles(const ExpandInput& in, platform::TileQuad2D* out, int capacity) {
    ExpandResult r;
    const LayerView& v = in.view;
    if (v.empty()) return r;
    const unsigned first = static_cast<unsigned>(in.first_id);
    const unsigned count = static_cast<unsigned>(in.tile_count);
    // The cell bits 13..15 are diagonal, flip y, flip x: the contract's orient bits once shifted down.
    static_assert(platform::kTileOrientDiagonal == mapfmt::kCellDiagonal >> 13);
    static_assert(platform::kTileOrientFlipY == mapfmt::kCellFlipY >> 13);
    static_assert(platform::kTileOrientFlipX == mapfmt::kCellFlipX >> 13);
    int py = v.origin_y + v.y0 * in.tile_h;
    for (int y = v.y0; y < v.y1; ++y, py += in.tile_h) {
        const std::uint16_t* row = in.cells + y * in.map_width;
        int px = v.origin_x + v.x0 * in.tile_w;
        for (int x = v.x0; x < v.x1; ++x, px += in.tile_w) {
            const std::uint16_t cell = row[x];
            const unsigned id = cell & mapfmt::kCellIdMask;
            if (id - first >= count) continue; // empty (id 0) or another tileset's tile
            if (r.written == capacity) {
                ++r.dropped;
                continue;
            }
            mapfmt::TileSource src = in.sources[id];
            if (in.flags != nullptr && (in.flags[id] & mapfmt::kTileAnimated) != 0) {
                const int a = find_animation(in.animations, in.animation_count, static_cast<int>(id));
                if (a >= 0) src = in.animated[a];
            }
            platform::TileQuad2D& q = out[r.written++];
            q.x = static_cast<std::int16_t>(px);
            q.y = static_cast<std::int16_t>(py);
            q.src_x = src.x;
            q.src_y = src.y;
            q.orient = static_cast<std::uint8_t>(cell >> 13);
        }
    }
    return r;
}

} // namespace nnh::detail
