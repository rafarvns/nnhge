// Visible cells of one (map layer, tileset) → tile quads (tilemap/tile_expand.hpp; spec 012 FR-2, FR-3).

#include "nnh/platform/video.hpp"
#include "test.hpp"
#include "tilemap/map_format.hpp"
#include "tilemap/tile_expand.hpp"

#include <cstdint>

namespace fmt = nnh::detail::mapfmt;
using nnh::detail::expand_tiles;
using nnh::detail::ExpandInput;
using nnh::detail::ExpandResult;
using nnh::detail::LayerView;
using nnh::platform::TileQuad2D;

namespace {

constexpr int kW = 4;
constexpr int kH = 3;

/// A 4 × 3 layer; ids 1..4 belong to tileset A, 5..6 to tileset B. Source of id n: (n·16, 100 + n).
struct Layer {
    std::uint16_t cells[kW * kH] = {};
    fmt::TileSource sources[8] = {};
    TileQuad2D out[32] = {};

    Layer() {
        for (int id = 1; id < 8; ++id) {
            sources[id] = {static_cast<std::uint16_t>(id * 16), static_cast<std::uint16_t>(100 + id)};
        }
    }
    void set(int x, int y, int id, bool fx = false, bool fy = false, bool diagonal = false) {
        cells[y * kW + x] = fmt::encode_cell(id, fx, fy, diagonal);
    }
    ExpandInput input(LayerView view, int first = 1, int count = 4) const {
        return {.cells = cells,
                .map_width = kW,
                .sources = sources,
                .first_id = first,
                .tile_count = count,
                .view = view,
                .tile_w = 16,
                .tile_h = 16};
    }
    ExpandResult expand(LayerView view, int first = 1, int count = 4, int capacity = 32) {
        return expand_tiles(input(view, first, count), out, capacity);
    }
};

constexpr LayerView kAll{.origin_x = 0, .origin_y = 0, .x0 = 0, .y0 = 0, .x1 = kW, .y1 = kH};

} // namespace

NNH_TEST(empty_cells_make_no_quads) {
    Layer l;
    const ExpandResult r = l.expand(kAll);
    CHECK(r.written == 0 && r.dropped == 0);
}

NNH_TEST(each_tile_becomes_a_quad_at_its_cell_in_row_order) {
    Layer l;
    l.set(2, 0, 3);
    l.set(1, 2, 1);
    l.set(0, 1, 4);
    const ExpandResult r = l.expand(kAll);
    CHECK(r.written == 3);
    CHECK(l.out[0].x == 32 && l.out[0].y == 0);
    CHECK(l.out[1].x == 0 && l.out[1].y == 16);
    CHECK(l.out[2].x == 16 && l.out[2].y == 32);
}

NNH_TEST(a_quad_takes_its_source_from_the_tile_table) {
    Layer l;
    l.set(0, 0, 3);
    CHECK(l.expand(kAll).written == 1);
    CHECK(l.out[0].src_x == 48 && l.out[0].src_y == 103);
    CHECK(l.out[0].orient == 0);
}

NNH_TEST(positions_start_at_the_view_origin) {
    Layer l;
    l.set(0, 0, 1);
    l.set(3, 2, 2);
    LayerView v = kAll;
    v.origin_x = -7;
    v.origin_y = 20;
    CHECK(l.expand(v).written == 2);
    CHECK(l.out[0].x == -7 && l.out[0].y == 20);
    CHECK(l.out[1].x == -7 + 48 && l.out[1].y == 20 + 32);
}

NNH_TEST(only_cells_inside_the_view_range_are_expanded) {
    Layer l;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x)
            l.set(x, y, 1);
    }
    const LayerView v{.origin_x = 0, .origin_y = 0, .x0 = 1, .y0 = 1, .x1 = 3, .y1 = 2};
    CHECK(l.expand(v).written == 2);
    CHECK(l.out[0].x == 16 && l.out[0].y == 16);
    CHECK(l.out[1].x == 32 && l.out[1].y == 16);
    CHECK(l.expand(LayerView{}).written == 0); // an empty view
}

NNH_TEST(flip_bits_become_the_quad_orientation) {
    Layer l;
    l.set(0, 0, 1, true, false, false);
    l.set(1, 0, 1, false, true, false);
    l.set(2, 0, 1, false, false, true);
    l.set(3, 0, 1, true, true, true);
    CHECK(l.expand(kAll).written == 4);
    namespace p = nnh::platform;
    CHECK(l.out[0].orient == p::kTileOrientFlipX);
    CHECK(l.out[1].orient == p::kTileOrientFlipY);
    CHECK(l.out[2].orient == p::kTileOrientDiagonal);
    CHECK(l.out[3].orient == (p::kTileOrientFlipX | p::kTileOrientFlipY | p::kTileOrientDiagonal));
    CHECK(l.out[3].src_x == 16); // flips don't change the source
}

NNH_TEST(each_tileset_pass_takes_only_its_own_ids) {
    Layer l;
    l.set(0, 0, 4); // last id of A
    l.set(1, 0, 5); // first id of B
    l.set(2, 0, 6);
    l.set(3, 0, 1); // first id of A
    CHECK(l.expand(kAll, 1, 4).written == 2);
    CHECK(l.out[0].x == 0 && l.out[1].x == 48);
    CHECK(l.expand(kAll, 5, 2).written == 2);
    CHECK(l.out[0].x == 16 && l.out[0].src_x == 80 && l.out[1].x == 32);
}

NNH_TEST(non_square_tiles_step_by_their_own_width_and_height) {
    Layer l;
    l.set(1, 2, 1);
    ExpandInput in = l.input(kAll);
    in.tile_w = 32;
    in.tile_h = 8;
    CHECK(expand_tiles(in, l.out, 32).written == 1);
    CHECK(l.out[0].x == 32 && l.out[0].y == 16);
}

NNH_TEST(quads_past_the_capacity_are_dropped_and_counted) {
    Layer l;
    for (int x = 0; x < kW; ++x)
        l.set(x, 0, 2);
    l.set(0, 2, 5); // another tileset: neither written nor dropped
    const ExpandResult r = l.expand(kAll, 1, 4, 3);
    CHECK(r.written == 3 && r.dropped == 1);
    CHECK(l.out[2].x == 32);
    const ExpandResult none = l.expand(kAll, 1, 4, 0);
    CHECK(none.written == 0 && none.dropped == 4);
}

NNH_TEST(an_animated_tile_takes_the_source_of_its_current_frame) {
    Layer l;
    std::uint8_t flags[8] = {};
    flags[2] = fmt::kTileAnimated;
    const fmt::AnimationEntry animations[] = {{.tile_id = 2, .frame_count = 1, .first_frame = 0}};
    const fmt::TileSource current[] = {{200, 201}};
    l.set(0, 0, 2, true, false, false);
    l.set(1, 0, 3);
    ExpandInput in = l.input(kAll);
    in.flags = flags;
    in.animations = animations;
    in.animation_count = 1;
    in.animated = current;
    CHECK(expand_tiles(in, l.out, 32).written == 2);
    CHECK(l.out[0].src_x == 200 && l.out[0].src_y == 201);
    CHECK(l.out[0].orient == nnh::platform::kTileOrientFlipX); // the cell's flips stay
    CHECK(l.out[1].src_x == 48 && l.out[1].src_y == 103);       // not animated: its own source
}

NNH_TEST(a_flagged_tile_without_an_animation_in_the_pass_keeps_its_source) {
    Layer l;
    std::uint8_t flags[8] = {};
    flags[2] = fmt::kTileAnimated;
    const fmt::AnimationEntry animations[] = {{.tile_id = 3, .frame_count = 1, .first_frame = 0}};
    const fmt::TileSource current[] = {{200, 201}};
    l.set(0, 0, 2);
    ExpandInput in = l.input(kAll);
    in.flags = flags;
    in.animations = animations;
    in.animation_count = 1;
    in.animated = current;
    CHECK(expand_tiles(in, l.out, 32).written == 1);
    CHECK(l.out[0].src_x == 32 && l.out[0].src_y == 102);
}
