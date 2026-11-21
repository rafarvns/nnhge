// Tile batches through the frame: recorded among sprites, expanded at submit, sent with draw_tiles between
// draw_2d passes, counted against AppConfig::gfx.max_tiles (spec 012 FR-3, FR-11, decision D26). The batch's
// expand function is a test stand-in: gfx never knows what a tilemap is.

#include "gfx/gfx_state.hpp"
#include "gfx_fixture.hpp"
#include "nnh/build.hpp"

#include <cstring>

namespace fake = nnh::platform_fake;
using gfx_test::load;
using gfx_test::restart;
using nnh::Screen;
using nnh::Vec2;
using nnh::detail::TileBatchRecord;
using nnh::detail::TileExpandRequest;
using nnh::detail::TileExpandResult;

namespace {

TileExpandRequest g_request;
int g_source = 0;

/// Writes `arg` quads at x = 0, 10, 20, ...
TileExpandResult quads(const TileExpandRequest& r) {
    g_request = r;
    TileExpandResult result;
    for (int i = 0; i < r.arg; ++i) {
        if (result.written == r.capacity) {
            ++result.dropped;
            continue;
        }
        r.out[result.written++] = {static_cast<std::int16_t>(i * 10), 5, 16, 0, 0};
    }
    return result;
}

TileBatchRecord batch(const nnh::Texture& texture, int count) {
    TileBatchRecord r;
    r.expand = quads;
    r.source = &g_source;
    r.arg = count;
    r.texture_slot = nnh::TextureAccess::slot(texture);
    r.texture_generation = nnh::TextureAccess::generation(texture);
    r.tile_w = 16;
    r.tile_h = 16;
    r.color = nnh::rgba(255, 255, 255, 128);
    return r;
}

const char* calls() {
    return fake::state().last_frame_calls;
}
const fake::FakeTiles& tiles() {
    return fake::state().last_frame_tiles;
}

nnh::Texture* g_tex = nullptr;
void overfill() {
    nnh::detail::gfx_record_tiles(Screen::Top, 0, batch(*g_tex, 1));
}
void frame_over_budget() {
    nnh::detail::gfx_record_tiles(Screen::Top, 0, batch(*g_tex, 20));
    (void)nnh::frame();
}

} // namespace

NNH_TEST(a_tile_batch_is_drawn_between_the_sprites_below_and_above_it) {
    restart({.gfx = {.max_tiles = 64}});
    nnh::Texture tex = load("tile");
    CHECK(nnh::frame());
    nnh::gfx::fill_rect(Screen::Top, {1, 0, 1, 1}, nnh::colors::red, {.layer = 2});
    nnh::detail::gfx_record_tiles(Screen::Top, 1, batch(tex, 3));
    nnh::gfx::fill_rect(Screen::Top, {2, 0, 1, 1}, nnh::colors::red, {.layer = 0});
    CHECK(nnh::frame());
    CHECK(std::strcmp(calls(), "TtT") == 0);
    CHECK(fake::state().last_frame_item_count == 2); // each 2D item once: one before the batch, one after
    CHECK(fake::state().last_frame_items[0].x == 2 && fake::state().last_frame_items[1].x == 1);
    CHECK(tiles().batch_count == 1 && tiles().quad_count == 3);
    const auto& b = tiles().batches[0];
    CHECK(b.screen == Screen::Top && b.batch.count == 3 && b.batch.layer == 1);
    CHECK(b.batch.texture != 0 && b.batch.tile_w == 16 && b.batch.tile_h == 16);
    CHECK(b.batch.color.a == 128);
    CHECK(tiles().quads[2].x == 20 && tiles().quads[2].src_x == 16);
    nnh::shutdown();
}

NNH_TEST(a_tile_batch_alone_on_a_screen_makes_no_empty_2d_pass) {
    restart({.gfx = {.max_tiles = 64}});
    nnh::Texture tex = load("tile");
    CHECK(nnh::frame());
    nnh::detail::gfx_record_tiles(Screen::Bottom, 0, batch(tex, 1));
    CHECK(nnh::frame());
    CHECK(std::strcmp(calls(), "b") == 0);
    nnh::shutdown();
}

NNH_TEST(tile_batches_use_the_camera_at_the_end_of_the_frame) {
    restart({.gfx = {.max_tiles = 64}});
    nnh::Texture tex = load("tile");
    CHECK(nnh::frame());
    TileBatchRecord r = batch(tex, 1);
    r.offset = {7, 8};
    nnh::detail::gfx_record_tiles(Screen::Top, 0, r);
    nnh::gfx::set_camera(Screen::Top, {{100, 50}}); // moved after the draw call
    CHECK(nnh::frame());
    CHECK(g_request.camera == (Vec2{100, 50}) && g_request.offset == (Vec2{7, 8}));
    CHECK(g_request.screen == Screen::Top && g_request.source == &g_source);
    nnh::shutdown();
}

NNH_TEST(stats_count_the_tiles_of_the_last_frame) {
    restart({.gfx = {.max_tiles = 64}});
    nnh::Texture tex = load("tile");
    CHECK(nnh::frame());
    nnh::detail::gfx_record_tiles(Screen::Top, 0, batch(tex, 5));
    nnh::detail::gfx_record_tiles(Screen::Bottom, 0, batch(tex, 4));
    CHECK(nnh::frame());
    const nnh::gfx::Stats s = nnh::gfx::stats();
    CHECK(s.tiles_last_frame == 9 && s.max_tiles == 64 && s.dropped_tiles == 0);
    CHECK(s.draws_last_frame == 2); // a batch takes one draw-list command
    CHECK(nnh::frame());
    CHECK(nnh::gfx::stats().tiles_last_frame == 0);
    nnh::shutdown();
}

NNH_TEST(a_forgotten_source_draws_nothing) {
    restart({.gfx = {.max_tiles = 64}});
    nnh::Texture tex = load("tile");
    CHECK(nnh::frame());
    nnh::detail::gfx_record_tiles(Screen::Top, 0, batch(tex, 2));
    nnh::detail::gfx_forget_tiles(&g_source);
    CHECK(nnh::frame());
    CHECK(tiles().batch_count == 0 && calls()[0] == '\0');
    nnh::shutdown();
}

NNH_TEST(tiles_over_max_tiles_assert_or_are_dropped_and_logged) {
    restart({.gfx = {.max_tiles = 16}});
    nnh::Texture tex = load("tile");
    g_tex = &tex;
    CHECK(nnh::frame());
    const bool crashed = fake::run_expecting_crash(frame_over_budget);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) CHECK(std::strstr(fake::last_crash().top, "max_tiles") != nullptr);
    if (!nnh::build::kAssertsEnabled) {
        CHECK(tiles().quad_count == 16);
        CHECK(nnh::gfx::stats().dropped_tiles == 4);
        CHECK(std::strstr(fake::state().last_console_line, "max_tiles") != nullptr);
    }
    nnh::shutdown();
}

NNH_TEST(drawing_tiles_without_max_tiles_asserts_with_the_fix) {
    restart(); // max_tiles defaults to 0: games without maps pay nothing
    CHECK(nnh::gfx::stats().max_tiles == 0);
    nnh::Texture tex = load("tile");
    g_tex = &tex;
    CHECK(nnh::frame());
    const bool crashed = fake::run_expecting_crash(overfill);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) CHECK(std::strstr(fake::last_crash().top, "max_tiles") != nullptr);
    CHECK(nnh::frame());
    CHECK(tiles().batch_count == 0);
    nnh::shutdown();
}

NNH_TEST(more_tile_batches_than_a_frame_holds_assert_or_are_dropped) {
    restart({.gfx = {.max_tiles = 4096}});
    nnh::Texture tex = load("tile");
    g_tex = &tex;
    CHECK(nnh::frame());
    for (int i = 0; i < nnh::detail::kMaxTileBatches; ++i)
        overfill();
    const bool crashed = fake::run_expecting_crash(overfill);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) CHECK(std::strstr(fake::last_crash().top, "tile batches") != nullptr);
    nnh::shutdown();
}
