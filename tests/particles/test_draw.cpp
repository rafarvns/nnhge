// Drawing through the frame (spec 024 FR-19; T4.3): one command per draw, sorted with sprites and tile batches by
// layer and call order; read at submit; an emitter destroyed after its draw call is forgotten, a moved one still drawn;
// a destroyed texture asserts and is skipped; both screens; drawn twice.

#include <cstring>

#include "draw_fixture.hpp"
#include "gfx/gfx_state.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;
using nnh::Screen;
using nnh::Vec2;
namespace fake = nnh::platform_fake;

namespace {

ParticleDesc squares() {
    ParticleDesc d;
    d.size = {2, 2};
    d.lifetime = {nnh::Frames{100}, nnh::Frames{100}};
    return d;
}

const char* calls() { return fake::state().last_frame_calls; }

int g_source = 0;
nnh::detail::TileExpandResult one_quad(const nnh::detail::TileExpandRequest& r) {
    nnh::detail::TileExpandResult result;
    if (r.capacity > 0) r.out[result.written++] = {0, 0, 0, 0, 0};
    return result;
}

ParticleEmitter* g_e = nullptr;
nnh::Texture* g_tex = nullptr;
/// A handle whose texture is gone (its slot's generation moved on), as one kept across a destroy: refused at the
/// draw call, as gfx::draw refuses it (a texture destroyed during the frame stays until its end).
void frame_with_destroyed_texture() {
    nnh::Texture stale = nnh::TextureAccess::make(nnh::TextureAccess::slot(*g_tex),
                                                  nnh::TextureAccess::generation(*g_tex) + 1, 16, 16);
    g_e->draw(Screen::Top, stale);
    nnh::TextureAccess::release(stale); // not ours to free
    (void)nnh::frame();
}

} // namespace

NNH_TEST(particle_draw_sits_between_sprites_and_tiles_by_layer_and_call) {
    gfx_test::restart({.gfx = {.max_tiles = 16, .max_particles = 64}});
    nnh::Texture tile = gfx_test::load("tile");
    ParticleEmitter e = particles_test::make(squares(), 8);
    e.emit_at(Vec2{50, 60}, 2);
    CHECK(nnh::frame());
    nnh::gfx::fill_rect(Screen::Top, {1, 0, 1, 1}, nnh::colors::red, {.layer = 3});
    e.draw(Screen::Top, {.layer = 2});
    nnh::detail::TileBatchRecord t;
    t.expand = one_quad;
    t.source = &g_source;
    t.texture_slot = nnh::TextureAccess::slot(tile);
    t.texture_generation = nnh::TextureAccess::generation(tile);
    t.tile_w = t.tile_h = 16;
    nnh::detail::gfx_record_tiles(Screen::Top, 1, t);
    nnh::gfx::fill_rect(Screen::Top, {2, 0, 1, 1}, nnh::colors::red, {.layer = 0});
    CHECK(nnh::frame());
    // layer 0 rect, layer 1 tiles, layer 2 particles (one draw_2d), layer 3 rect.
    CHECK(std::strcmp(calls(), "TtTT") == 0);
    const auto& items = fake::state().last_frame_items;
    CHECK(fake::state().last_frame_item_count == 4);
    CHECK(items[0].x == 2 && items[1].x == 49 && items[1].layer == 2 && items[3].x == 1);
    CHECK(nnh::gfx::stats().particles_last_frame == 2 && nnh::gfx::stats().draws_last_frame == 4);
    nnh::shutdown();
}

NNH_TEST(particle_draw_is_one_command) {
    gfx_test::restart({.gfx = {.max_draws = 16, .max_particles = 512}});
    ParticleEmitter e = particles_test::make(squares(), 512);
    e.emit_at(Vec2{100, 100}, 300);
    CHECK(nnh::frame());
    e.draw(Screen::Top);
    CHECK(nnh::frame());
    CHECK(nnh::gfx::stats().draws_last_frame == 1 && nnh::gfx::stats().particles_last_frame == 300);
    CHECK(std::strcmp(calls(), "TTT") == 0); // chunks of 128 items
    nnh::shutdown();
}

NNH_TEST(particles_are_read_at_submit) {
    gfx_test::restart({.gfx = {.max_particles = 64}});
    ParticleDesc d = squares();
    d.speed = {600, 600}; // 10 px per frame
    ParticleEmitter e = particles_test::make(d, 8);
    e.emit_at(Vec2{100, 100}, 1);
    CHECK(nnh::frame());
    e.draw(Screen::Top);
    e.update(); // after the draw call: the item shows the moved particle
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_items[0].x == 109); // 110 − 1 (a 2 px square)
    nnh::shutdown();
}

NNH_TEST(emitter_destroyed_after_its_draw_call_draws_nothing_moved_one_still_draws) {
    gfx_test::restart({.gfx = {.max_particles = 64}});
    CHECK(nnh::frame());
    {
        ParticleEmitter gone = particles_test::make(squares(), 8);
        gone.emit_at(Vec2{10, 10}, 3);
        gone.draw(Screen::Top);
    }
    ParticleEmitter a = particles_test::make(squares(), 8);
    a.emit_at(Vec2{30, 30}, 2);
    a.draw(Screen::Top);
    ParticleEmitter b = static_cast<ParticleEmitter&&>(a);
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 2 && fake::state().last_frame_items[0].x == 29);
    nnh::shutdown();
}

NNH_TEST(particles_drawn_with_a_destroyed_texture_assert_and_are_skipped) {
    gfx_test::restart({.gfx = {.max_particles = 64}});
    nnh::Texture tex = gfx_test::load("tile");
    ParticleEmitter e = particles_test::make(squares(), 8);
    e.emit_at(Vec2{30, 30}, 2);
    g_e = &e;
    g_tex = &tex;
    CHECK(nnh::frame());
    const bool crashed = fake::run_expecting_crash(frame_with_destroyed_texture);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) std::printf("TOP: %s|\n", fake::last_crash().top);
    if (crashed) CHECK(std::strstr(fake::last_crash().top, "texture") != nullptr);
    if (!crashed) CHECK(fake::state().last_frame_item_count == 0);
    nnh::shutdown();
}

NNH_TEST(emitter_drawn_on_both_screens_and_twice) {
    gfx_test::restart({.gfx = {.max_particles = 64}});
    ParticleEmitter e = particles_test::make(squares(), 8);
    e.emit_at(Vec2{30, 30}, 2);
    CHECK(nnh::frame());
    e.draw(Screen::Top);
    e.draw(Screen::Top, {.layer = 5, .ignore_camera = true});
    e.draw(Screen::Bottom);
    CHECK(nnh::frame());
    CHECK(std::strcmp(calls(), "TTB") == 0);
    CHECK(fake::state().last_frame_item_count == 6 && nnh::gfx::stats().particles_last_frame == 6);
    CHECK(e.stats().drawn == 6);
    nnh::shutdown();
}

NNH_TEST(particle_tint_and_opacity_multiply_every_particle) {
    gfx_test::restart({.gfx = {.max_particles = 64}});
    ParticleDesc d = squares();
    d.color_min = d.color_max = nnh::rgba(255, 128, 64, 255);
    ParticleEmitter e = particles_test::make(d, 8);
    e.emit(1);
    CHECK(nnh::frame());
    e.draw(Screen::Top, {.tint = nnh::rgba(128, 255, 255, 255), .opacity = 0.5f});
    CHECK(nnh::frame());
    const nnh::Color c = fake::state().last_frame_items[0].color;
    CHECK(c.r == 128 && c.g == 128 && c.b == 64 && c.a == 128);
    nnh::shutdown();
}
