// Zoom 1 draws exactly what the renderer drew before spec 018 (AC-3, decision D33): a hash of every field of every
// recorded DrawItem2D, TileBatch2D and TileQuad2D of a rich scene over three frames, pinned on the renderer before
// zoom existed (T0.2). Named fields only, never raw bytes (padding). If this fails, zoom 1 changed a pixel.

#include "../tilemap/map_fixture.hpp"
#include "nnh/platform/video.hpp"

#include <bit>
#include <cstdint>
#include <cstdio>

namespace fake = nnh::platform_fake;
using map_test::layer;
using map_test::make;
using map_test::tile;
using map_test::tileset;
using nnh::Screen;
namespace gfx = nnh::gfx;

namespace {

struct Hash {
    std::uint32_t h = 2166136261u;
    void byte(std::uint8_t b) {
        h ^= b;
        h *= 16777619u;
    }
    void u32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void i32(int v) { u32(static_cast<std::uint32_t>(v)); }
    void f(float v) { u32(std::bit_cast<std::uint32_t>(v)); }
    void color(nnh::Color c) {
        byte(c.r);
        byte(c.g);
        byte(c.b);
        byte(c.a);
    }
};

void hash_item(Hash& h, const nnh::platform::DrawItem2D& i) {
    h.byte(static_cast<std::uint8_t>(i.kind));
    h.byte(i.layer);
    h.byte(i.flip_x ? 1 : 0);
    h.byte(i.flip_y ? 1 : 0);
    h.byte(i.texture != 0 ? 1 : 0); // handles differ between runs; only "has one" matters
    h.i32(i.region.x);
    h.i32(i.region.y);
    h.i32(i.region.w);
    h.i32(i.region.h);
    h.f(i.x);
    h.f(i.y);
    h.f(i.w);
    h.f(i.h);
    h.f(i.pivot_x);
    h.f(i.pivot_y);
    h.f(i.rotation);
    h.color(i.color);
}

void hash_frame(Hash& h) {
    const fake::State& s = fake::state();
    h.i32(s.last_frame_item_count);
    for (int i = 0; i < s.last_frame_item_count && i < fake::kMaxRecordedItems; ++i) hash_item(h, s.last_frame_items[i]);
    for (const char* c = s.last_frame_calls; *c != 0; ++c) h.byte(static_cast<std::uint8_t>(*c));
    const fake::FakeTiles& t = s.last_frame_tiles;
    h.i32(t.batch_count);
    for (int b = 0; b < t.batch_count && b < fake::kMaxRecordedTileBatches; ++b) {
        const fake::FakeTileBatch& fb = t.batches[b];
        h.byte(static_cast<std::uint8_t>(fb.screen));
        h.byte(fb.batch.texture != 0 ? 1 : 0);
        h.i32(fb.batch.tile_w);
        h.i32(fb.batch.tile_h);
        h.color(fb.batch.color);
        h.byte(fb.batch.layer);
        h.i32(fb.batch.count);
        h.i32(fb.first_quad);
        // Spec 018 T5.1 added the destination size: at zoom 1 it is the tile size (kept out of the pinned hash).
        CHECK(fb.batch.dst_w == fb.batch.tile_w && fb.batch.dst_h == fb.batch.tile_h);
    }
    h.i32(t.quad_count);
    for (int q = 0; q < t.quad_count && q < fake::kMaxRecordedTileQuads; ++q) {
        const nnh::platform::TileQuad2D& quad = t.quads[q];
        h.i32(quad.x);
        h.i32(quad.y);
        h.i32(quad.src_x);
        h.i32(quad.src_y);
        h.byte(quad.orient);
    }
}

/// Two maps (16×16 and 8×8 tiles) with parallax, offsets, flips (diagonal too) and an animated tile.
struct World {
    nnh::Texture hero; // 64×32
    nnh::Texture tile16;
    nnh::Tilemap big;   // 16×16 tiles, 12×8 cells, 3 layers
    nnh::Tilemap small; // 8×8 tiles, 10×6 cells, 2 layers

    World() {
        gfx_test::restart({.gfx = {.max_tiles = 2048}});
        hero = gfx_test::load("hero");
        tile16 = gfx_test::load("tile");
        big = make({.width = 12, .height = 8, .max_animations = 2, .max_animation_frames = 4});
        CHECK(tileset(big, {.texture = &hero}) == 1); // ids 1..8
        CHECK(tileset(big, {.texture = &tile16}) == 9);
        const int back = layer(big, {.name = "back", .parallax = {0.5f, 0.75f}});
        const int ground = layer(big, {.name = "ground", .offset = {3.5f, -2.25f}});
        const int front = layer(big, {.name = "front"});
        big.fill(back, 0, 0, 12, 8, tile(2));
        for (int x = 0; x < 12; ++x) {
            big.set_tile(ground, x, 5, tile(1 + x % 8, x % 2 == 1, x % 3 == 1, x % 4 == 2));
            big.set_tile(ground, x, 7, tile(x % 5 == 0 ? 9 : 3));
        }
        const nnh::Tilemap::AnimationFrame water[] = {{.tile_id = 6, .ms = 16}, {.tile_id = 7, .ms = 16}};
        CHECK(big.set_tile_animation(6, water, 2).ok());
        big.fill(front, 4, 2, 3, 2, tile(6));
        big.set_parallax_origin({37.5f, 11.25f});

        small = make({.width = 10, .height = 6, .tile_width = 8, .tile_height = 8});
        CHECK(tileset(small, {.texture = &hero}) == 1); // 32 tiles of 8×8
        const int s0 = layer(small, {.parallax = {1.25f, 1}});
        const int s1 = layer(small, {.offset = {-1.5f, 0.5f}});
        small.fill(s0, 0, 0, 10, 6, tile(5));
        for (int x = 0; x < 10; ++x) small.set_tile(s1, x, x % 6, tile(10 + x, x % 2 == 0, false, x % 3 == 0));
    }
    ~World() {
        big.reset();
        small.reset();
        hero.reset();
        tile16.reset();
        nnh::shutdown();
    }

    void draw(int frame) const {
        const float f = static_cast<float>(frame);
        // Sprites: sub-pixel and negative positions, scale, negative scale, flips, pivot, rotation, tint/opacity.
        gfx::draw(Screen::Top, hero, {10.25f + f, 20.5f}, {.layer = 3});
        gfx::draw(Screen::Top, hero, {-7.5f, -3.49f - f}, {.region = {16, 0, 16, 16}, .flip_x = true, .layer = 4});
        gfx::draw(Screen::Top, hero, {120.7f, 64.5f},
                  {.region = {0, 16, 32, 16}, .pivot = {16, 8}, .rotation = 0.5f + f, .scale = {2, -1.5f}});
        gfx::draw(Screen::Top, hero, {200.5f, 100.5f},
                  {.pivot = {3.5f, 2}, .scale = {-0.5f, 3}, .flip_y = true, .tint = nnh::rgb(10, 200, 30),
                   .opacity = 0.4f, .layer = 6});
        gfx::draw(Screen::Top, tile16, {380.49f, 230.51f}, {.layer = 15, .ignore_camera = true});
        // Shapes, camera-relative and HUD.
        gfx::fill_rect(Screen::Top, {30.3f, 40.6f, 20.5f, 9.49f}, nnh::colors::red, {.layer = 2});
        gfx::draw_rect(Screen::Top, {-5.5f, 12.5f, 33.3f, 17.7f}, nnh::colors::blue, {.layer = 2});
        gfx::draw_line(Screen::Top, {1.5f, 2.5f}, {99.4f, -30.6f + f}, nnh::colors::yellow, {.layer = 7});
        gfx::fill_rect(Screen::Top, {0, 0, 400, 12}, nnh::rgba(0, 0, 0, 128), {.layer = 14, .ignore_camera = true});
        // Text: shadow, scale 2, centred, and a HUD line.
        gfx::draw_text(Screen::Top, {150.5f, 30.25f}, "Ab", {.scale = 2, .align = nnh::TextAlign::Center,
                                                             .shadow = true, .layer = 8});
        gfx::draw_text(Screen::Top, {2, 2}, "Z", {.layer = 15, .ignore_camera = true});
        gfx::draw_tilemap(Screen::Top, big, {.layer = 1, .offset = {0.5f, 0.25f}});
        // Bottom screen.
        gfx::draw(Screen::Bottom, hero, {50.5f, 60.5f}, {.region = {8, 8, 8, 8}, .scale = {4, 4}, .layer = 2});
        gfx::draw_rect(Screen::Bottom, {10, 10, 100.5f, 50.5f}, nnh::colors::green);
        gfx::draw_line(Screen::Bottom, {300, 5}, {3.5f, 230.5f}, nnh::colors::white, {.ignore_camera = true});
        gfx::draw_text(Screen::Bottom, {40.5f, 200.75f}, "q", {.shadow = true});
        gfx::draw_tilemap(Screen::Bottom, small, {.layer = 0});
    }
};

} // namespace

NNH_TEST(zoom_1_draws_the_same_items_batches_and_quads_as_before_zoom) {
    World w;
    const nnh::Vec2 top[3] = {{13.3f, 7.6f}, {-21.5f, 2.5f}, {40.49f, -11.51f}};
    const nnh::Vec2 bottom[3] = {{-3.75f, 8.25f}, {5.5f, -0.5f}, {17.1f, 3.9f}};
    Hash h;
    CHECK(nnh::frame());
    for (int frame = 0; frame < 3; ++frame) {
        gfx::set_camera(Screen::Top, {top[frame]});
        gfx::set_camera(Screen::Bottom, {bottom[frame]});
        w.draw(frame);
        CHECK(nnh::frame());
        CHECK(fake::state().last_frame_item_count <= fake::kMaxRecordedItems);
        CHECK(fake::state().last_frame_tiles.quad_count <= fake::kMaxRecordedTileQuads);
        CHECK(fake::state().last_frame_item_count == 19 && fake::state().last_frame_tiles.batch_count == 6);
        CHECK(fake::state().last_frame_tiles.quad_count > 140);
        hash_frame(h);
    }
    constexpr std::uint32_t kGolden = 0x276C0CD5; // pinned on the renderer before zoom (T0.2)
    if (h.h != kGolden) std::printf("zoom-1 regression hash: 0x%08X\n", static_cast<unsigned>(h.h));
    CHECK(h.h == kGolden);
}
