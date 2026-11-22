// Zoomed expansion (tilemap/tile_expand.hpp; spec 018 FR-9, decision D33): quads on the zoomed edges,
// widen/taller bits where a step is one pixel more, row chunks when the scratch fills, drops when the budget
// is spent.

#include "nnh/platform/video.hpp"
#include "test.hpp"
#include "tilemap/map_format.hpp"
#include "tilemap/tile_expand.hpp"
#include "tilemap/zoom_edges.hpp"

#include <cstdint>

namespace fmt = nnh::detail::mapfmt;
namespace platform = nnh::platform;
using nnh::detail::expand_tiles;
using nnh::detail::ExpandInput;
using nnh::detail::ExpandResult;
using nnh::detail::LayerView;
using nnh::detail::zoom_q;
using platform::TileQuad2D;

namespace {

constexpr int kW = 6;
constexpr int kH = 4;

/// A 6 × 4 layer full of id 1 (one tileset of ids 1..2), 10 × 10 tiles.
struct Layer {
    std::uint16_t cells[kW * kH] = {};
    fmt::TileSource sources[4] = {};
    TileQuad2D out[64] = {};

    Layer() {
        for (std::uint16_t& c : cells)
            c = fmt::encode_cell(1, false, false, false);
        sources[1] = {10, 0};
        sources[2] = {20, 0};
    }
    ExpandResult expand(float zoom, int capacity = 64, int first_row = 0, bool chunk = false,
                        int origin = 0) {
        LayerView v{.origin_x = origin,
                    .origin_y = origin,
                    .x0 = 0,
                    .y0 = 0,
                    .x1 = kW,
                    .y1 = kH,
                    .zoom_q = zoom_q(zoom)};
        const ExpandInput in{.cells = cells,
                             .map_width = kW,
                             .sources = sources,
                             .first_id = 1,
                             .tile_count = 2,
                             .view = v,
                             .tile_w = 10,
                             .tile_h = 10,
                             .first_row = first_row,
                             .chunk = chunk};
        return expand_tiles(in, out, capacity);
    }
};

} // namespace

NNH_TEST(integer_zooms_place_quads_on_exact_multiples_without_bits) {
    Layer l;
    const ExpandResult r = l.expand(3, 64, 0, false, -7);
    CHECK(r.written == kW * kH && r.dropped == 0 && r.next_row == -1);
    for (int i = 0; i < r.written; ++i) {
        CHECK(l.out[i].x == -7 + (i % kW) * 30 && l.out[i].y == -7 + (i / kW) * 30);
        CHECK(l.out[i].orient == 0 && l.out[i].src_x == 10);
    }
}

NNH_TEST(fractional_zooms_mark_the_wider_and_taller_tiles) {
    Layer l;
    l.cells[1] = fmt::encode_cell(2, true, false, true); // flips are kept beside the new bits
    const ExpandResult r = l.expand(1.25f); // 12.5-pixel steps: 13, 12, 13, 12, ... (edges 0, 13, 25, 38)
    CHECK(r.written == kW * kH);
    CHECK(l.out[0].x == 0 && l.out[1].x == 13 && l.out[2].x == 25 && l.out[3].x == 38);
    CHECK(l.out[0].orient == (platform::kTileOrientWiden | platform::kTileOrientTaller));
    CHECK(l.out[1].orient ==
          (platform::kTileOrientFlipX | platform::kTileOrientDiagonal | platform::kTileOrientTaller));
    CHECK(l.out[1].src_x == 20);
    CHECK(l.out[kW].y == 13 && l.out[kW].orient == platform::kTileOrientWiden); // row 1 is 12 tall
}

NNH_TEST(each_quad_ends_where_the_next_begins) {
    const float zooms[] = {0.5f, 0.75f, 1.3f, 1.5f, 2.6f};
    for (float z : zooms) {
        Layer l;
        const int d = nnh::detail::zoomed_size(10, zoom_q(z));
        const ExpandResult r = l.expand(z);
        for (int i = 0; i + 1 < r.written; ++i) {
            if ((i + 1) % kW == 0) continue; // the row's last tile
            const int w = d + ((l.out[i].orient & platform::kTileOrientWiden) != 0 ? 1 : 0);
            CHECK(l.out[i].x + w == l.out[i + 1].x);
        }
        for (int i = 0; i + kW < r.written; ++i) {
            const int h = d + ((l.out[i].orient & platform::kTileOrientTaller) != 0 ? 1 : 0);
            CHECK(l.out[i].y + h == l.out[i + kW].y);
        }
    }
}

NNH_TEST(a_full_scratch_cuts_the_batch_at_a_row_end_when_chunking) {
    Layer l;
    ExpandResult r = l.expand(0.5f, 15, 0, true); // rows of 6: two fit, the third is taken back
    CHECK(r.written == 12 && r.dropped == 0 && r.next_row == 2);
    r = l.expand(0.5f, 15, r.next_row, true);
    CHECK(r.written == 12 && r.next_row == -1);
    CHECK(l.out[0].y == 10); // row 2 at 5-pixel steps
}

NNH_TEST(without_chunking_the_tiles_past_the_budget_are_dropped_as_at_zoom_1) {
    Layer l;
    const ExpandResult r = l.expand(0.5f, 15, 0, false);
    CHECK(r.written == 15 && r.dropped == 9 && r.next_row == -1);
}
