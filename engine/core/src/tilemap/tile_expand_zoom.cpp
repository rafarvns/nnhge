// The zoomed expansion (spec 018 FR-9, decision D33; see tile_expand.hpp): quads placed on the zoomed tile edges,
// one pixel wider/taller where an edge step is floor(tile × zoom) + 1, cut in row chunks when the scratch fills.

#include "nnh/assert.hpp"
#include "tilemap/tile_animation.hpp"
#include "tilemap/tile_expand.hpp"
#include "tilemap/zoom_edges.hpp"

namespace nnh::detail {

ExpandResult expand_zoomed(const ExpandInput& in, platform::TileQuad2D* out, int capacity) {
    ExpandResult r;
    const LayerView& v = in.view;
    const int columns = v.x1 - v.x0;
    NNH_ASSERT(columns <= kMaxZoomCells, "tilemap: %d zoomed columns", columns);
    if (columns > kMaxZoomCells) return r; // impossible within the zoom and tile size limits
    std::int16_t xs[kMaxZoomCells + 1]; // 256 bytes of stack
    zoom_edges(v.origin_x, in.tile_w, v.zoom_q, v.x0, columns, xs);
    const int dst_w = zoomed_size(in.tile_w, v.zoom_q); // = TileBatch2D::dst_w
    const int dst_h = zoomed_size(in.tile_h, v.zoom_q);
    const unsigned first = static_cast<unsigned>(in.first_id);
    const unsigned count = static_cast<unsigned>(in.tile_count);
    const int y_start = in.first_row > v.y0 ? in.first_row : v.y0;
    for (int y = y_start; y < v.y1; ++y) {
        const int top = v.origin_y + zoom_edge(y, in.tile_h, v.zoom_q);
        const int bottom = v.origin_y + zoom_edge(y + 1, in.tile_h, v.zoom_q);
        const std::uint8_t taller = bottom - top > dst_h ? platform::kTileOrientTaller : 0;
        const std::uint16_t* row = in.cells + y * in.map_width;
        const int row_start = r.written;
        for (int x = v.x0; x < v.x1; ++x) {
            const std::uint16_t cell = row[x];
            const unsigned id = cell & mapfmt::kCellIdMask;
            if (id - first >= count) continue; // empty (id 0) or another tileset's tile
            if (r.written == capacity) {
                if (in.chunk && row_start > 0) { // the scratch is full: take the row back, resume there
                    r.written = row_start;
                    r.next_row = y;
                    return r;
                }
                ++r.dropped; // the frame's budget is spent (as at zoom 1)
                continue;
            }
            mapfmt::TileSource src = in.sources[id];
            if (in.flags != nullptr && (in.flags[id] & mapfmt::kTileAnimated) != 0) {
                const int a = find_animation(in.animations, in.animation_count, static_cast<int>(id));
                if (a >= 0) src = in.animated[a];
            }
            const int k = x - v.x0;
            platform::TileQuad2D& q = out[r.written++];
            q.x = xs[k];
            q.y = static_cast<std::int16_t>(top);
            q.src_x = src.x;
            q.src_y = src.y;
            const std::uint8_t widen = xs[k + 1] - xs[k] > dst_w ? platform::kTileOrientWiden : 0;
            q.orient = static_cast<std::uint8_t>((cell >> 13) | widen | taller);
        }
    }
    return r;
}

} // namespace nnh::detail
