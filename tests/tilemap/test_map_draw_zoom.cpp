// gfx::draw_tilemap at a camera zoom, end to end on the fake platform (spec 018 FR-9, AC-3, decision D33):
// full layers have no gap and no overlap at any zoom, integer zooms set no widen/taller bit, a parallax-1
// layer stays on sprites, and zoom 0.5 draws about four times the quads of zoom 1.

#include "map_fixture.hpp"
#include "nnh/platform/video.hpp"

namespace fake = nnh::platform_fake;
namespace platform = nnh::platform;
using map_test::layer;
using map_test::make;
using map_test::tile;
using map_test::tileset;
using nnh::Screen;
namespace gfx = nnh::gfx;

namespace {

/// A 40 × 30 map of 32 × 32 tiles by default (few enough quads for the fake's 512 at every zoom tested).
struct Scene {
    nnh::Texture sheet; // "hero" 64 × 32: two 32 × 32 tiles
    nnh::Tilemap map;
    int ground = -1;
    explicit Scene(int tile_size = 32, int w = 40, int h = 30) {
        gfx_test::restart({.gfx = {.max_tiles = 8000}});
        sheet = gfx_test::load("hero");
        map = make({.width = w, .height = h, .tile_width = tile_size, .tile_height = tile_size});
        CHECK(tileset(map, {.texture = &sheet}) == 1);
        ground = layer(map);
        map.fill(ground, 0, 0, w, h, tile(1));
    }
    ~Scene() {
        map.reset();
        sheet.reset();
        nnh::shutdown();
    }
    void frame(nnh::Camera2D camera) {
        gfx::set_camera(Screen::Top, camera);
        CHECK(nnh::frame());
        gfx::draw_tilemap(Screen::Top, map);
        CHECK(nnh::frame());
    }
};

const fake::FakeTiles& tiles() {
    return fake::state().last_frame_tiles;
}

int width_of(const platform::TileQuad2D& q, const platform::TileBatch2D& b) {
    return b.dst_w + ((q.orient & platform::kTileOrientWiden) != 0 ? 1 : 0);
}
int height_of(const platform::TileQuad2D& q, const platform::TileBatch2D& b) {
    return b.dst_h + ((q.orient & platform::kTileOrientTaller) != 0 ? 1 : 0);
}

} // namespace

NNH_TEST(zoomed_layers_cover_the_screen_without_gaps_or_overlaps) {
    const float zooms[] = {0.5f, 0.75f, 1.3f, 1.5f, 2, 3};
    for (float z : zooms) {
        Scene s;
        s.frame({{37.3f, 21.9f}, z});
        CHECK(tiles().batch_count == 1 && tiles().quad_count <= fake::kMaxRecordedTileQuads);
        const platform::TileBatch2D& b = tiles().batches[0].batch;
        const int n = b.count;
        // Columns: the row length is the number of quads before y changes.
        int columns = 0;
        while (columns < n && tiles().quads[columns].y == tiles().quads[0].y)
            ++columns;
        const int rows = n / columns;
        CHECK(columns * rows == n);
        int gaps = 0;
        for (int r = 0; r < rows; ++r) {
            const platform::TileQuad2D* row = tiles().quads + r * columns;
            if (row[0].x > 0 || row[columns - 1].x + width_of(row[columns - 1], b) < 400) ++gaps;
            for (int c = 0; c + 1 < columns; ++c) {
                if (row[c].x + width_of(row[c], b) != row[c + 1].x) ++gaps;
            }
            if (r + 1 < rows && row[0].y + height_of(row[0], b) != row[columns].y) ++gaps;
        }
        if (tiles().quads[0].y > 0) ++gaps;
        const platform::TileQuad2D& last = tiles().quads[n - 1];
        if (last.y + height_of(last, b) < 240) ++gaps;
        CHECK(gaps == 0);
    }
}

NNH_TEST(integer_zooms_set_no_widen_or_taller_bit) {
    for (int z = 2; z <= 4; ++z) {
        Scene s;
        s.frame({{13.7f, 5.2f}, static_cast<float>(z)});
        const platform::TileBatch2D& b = tiles().batches[0].batch;
        CHECK(b.dst_w == 32 * z && b.dst_h == 32 * z && b.tile_w == 32);
        for (int i = 0; i < tiles().quad_count; ++i)
            CHECK((tiles().quads[i].orient & 0x18) == 0);
    }
}

NNH_TEST(a_sprite_on_a_tile_corner_lands_on_the_quads_corner) {
    const float zooms[] = {2, 3, 1.5f, 0.75f};
    for (float z : zooms) {
        Scene s;
        gfx::set_camera(Screen::Top, {{37.3f, 21.9f}, z});
        CHECK(nnh::frame());
        gfx::draw_tilemap(Screen::Top, s.map);
        gfx::draw(Screen::Top, s.sheet, {64, 96}, {.region = {0, 0, 4, 4}}); // the corner of cell (2, 3)
        CHECK(nnh::frame());
        const platform::DrawItem2D& sprite = fake::state().last_frame_items[0];
        bool found = false;
        for (int i = 0; i < tiles().quad_count; ++i) {
            const platform::TileQuad2D& q = tiles().quads[i];
            const float dx = sprite.x - q.x, dy = sprite.y - q.y;
            const bool integer = z == static_cast<float>(static_cast<int>(z));
            const float tolerance = integer ? 0.0f : 1.0f;
            if (dx <= tolerance && -dx <= tolerance && dy <= tolerance && -dy <= tolerance) found = true;
        }
        CHECK(found);
    }
}

NNH_TEST(zoom_half_draws_about_four_times_the_quads_of_zoom_1) {
    Scene s(16, 80, 60);
    s.frame({{3, 3}, 1});
    const int one = gfx::stats().tiles_last_frame;
    s.frame({{3, 3}, 0.5f});
    const int half = gfx::stats().tiles_last_frame;
    CHECK(one == 26 * 16 && half == 51 * 31);
}
