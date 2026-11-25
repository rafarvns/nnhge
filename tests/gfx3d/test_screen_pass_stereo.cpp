// The top screen drawn once per eye (spec 026 FR-14/FR-15/FR-20/FR-28, T12.2) on the fake platform: the platform calls
// of a stereo frame, the same 2D items and debug pass in both eyes, per-eye matrices and lights, the culling union,
// stats per eye, tile budgets not spent twice, and mono frames exactly as before.

#include <cmath>
#include <cstring>
#include <utility>

#include "gfx/gfx_state.hpp"
#include "gfx3d/pass3d.hpp"
#include "gfx3d_fixture.hpp"
#include "nnh/debug.hpp"
#include "nnh/system.hpp"

using namespace gfx3d_test;
using nnh::Screen;
using nnh::Vec3;

namespace {

nnh::Mesh g_cube;

const char* last_calls() { return fake::state().last_frame_calls; }

/// 3D on with a stereo screen and the slider at `slider`.
void restart_stereo(float slider, int max_tiles = 0) {
    nnh::shutdown();
    fake::reset();
    fake::set_capabilities({nnh::Capability::SecondScreen, nnh::Capability::Stereo3D});
    fake::set_stereo_slider(slider);
    nnh::AppConfig config;
    config.gfx.max_draws_3d = 64;
    config.gfx.max_tiles = max_tiles;
    CHECK(nnh::init(config));
}

void hud_and_sky() {
    nnh::gfx::fill_rect(Screen::Top, {0, 0, 400, 240}, {10, 10, 80, 255}, {.layer = 2}); // sky
    nnh::gfx::fill_rect(Screen::Top, {4, 4, 20, 8}, {255, 255, 255, 255}, {.layer = 9});  // HUD
}

const nnh::platform_fake::Fake3DCall* nth_begin(int n) {
    const fake::Fake3D& l = fake::pass3d_log();
    for (int i = 0; i < l.last_frame_call_count; ++i)
        if (l.last_frame_calls[i].kind == 'b' && n-- == 0) return &l.last_frame_calls[i];
    return nullptr;
}

bool same_item(const nnh::platform::DrawItem2D& a, const nnh::platform::DrawItem2D& b) {
    return std::memcmp(&a, &b, sizeof a) == 0;
}

} // namespace

NNH_TEST(stereo_frame_draws_the_top_screen_once_per_eye) {
    restart_stereo(1.0f);
    g_cube = cube();
    next_frame();
    hud_and_sky();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    CHECK(std::strcmp(last_calls(), "LT[]TRT[]TL") == 0);
    const fake::Fake3D& l = fake::pass3d_log();
    CHECK(l.last_frame_stereo && l.last_frame_right_clears == 1);
    // The same 2D in both eyes (FR-15): sky, HUD, sky, HUD.
    const auto& items = fake::state().last_frame_items;
    CHECK(fake::state().last_frame_item_count == 4 && same_item(items[0], items[2]) && same_item(items[1], items[3]));
    const auto* left = nth_begin(0);
    const auto* right = nth_begin(1);
    CHECK(left != nullptr && right != nullptr && left->eye == 0 && right->eye == 1);
    if (left == nullptr || right == nullptr) return;
    CHECK(left->pass.eye == 0 && right->pass.eye == 1 && left->pass.below_2d && right->pass.below_2d);
    CHECK(left->pass.projection[8] > 0 && right->pass.projection[8] == -left->pass.projection[8]);
    const nnh::Stats3D s = nnh::gfx3d::stats();
    CHECK(s.eyes == 2 && s.triangles[0] == 12 && s.triangles[1] == 12 && s.draws == 1);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(stereo_draws_move_by_the_eye_offset) {
    restart_stereo(1.0f);
    g_cube = cube();
    next_frame();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    const nnh::platform::Draw3D* d[4];
    CHECK(last_draws(d, 4) == 2);
    // view x translation: −(−0.125) in the left eye, −0.125 in the right one (focal 5 / 20 × slider 1 / 2).
    CHECK(d[0]->model_view[3] - d[1]->model_view[3] == 0.25f);
    CHECK(fake::pass3d_log().last_frame_calls[1].eye == 0);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(stereo_lights_are_in_each_eyes_view) {
    restart_stereo(1.0f);
    g_cube = cube();
    next_frame();
    nnh::Lighting lights;
    lights.count = 1;
    lights.lights[0] = {.kind = nnh::LightKind::Point, .vector = {1, 0, 0}, .range = 4};
    nnh::gfx3d::set_lighting(Screen::Top, lights);
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    const auto* left = nth_begin(0);
    const auto* right = nth_begin(1);
    CHECK(left != nullptr && right != nullptr);
    if (left == nullptr || right == nullptr) return;
    CHECK(left->pass.lights[0].vector[0] == 1.125f && right->pass.lights[0].vector[0] == 0.875f);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(stereo_debug_pass_is_drawn_in_both_eyes_last) {
    restart_stereo(1.0f);
    g_cube = cube();
    next_frame();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    nnh::debug::print(Screen::Top, 2, 2, "hi");
    next_frame();
    CHECK(std::strcmp(last_calls(), "L[]R[]LTRTL") == 0);
    const int n = fake::state().last_frame_item_count;
    CHECK(n == 4 && same_item(fake::state().last_frame_items[0], fake::state().last_frame_items[2]));
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(stereo_slider_at_zero_draws_one_eye_as_before) {
    restart_stereo(0.0f);
    g_cube = cube();
    next_frame();
    hud_and_sky();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    CHECK(std::strcmp(last_calls(), "T[]T") == 0);
    CHECK(!fake::pass3d_log().last_frame_stereo && nnh::gfx3d::stats().eyes == 1);
    CHECK(nth_begin(0) != nullptr && nth_begin(0)->pass.projection[8] == 0.0f);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(stereo_frames_without_3d_draws_show_one_picture) {
    restart_stereo(1.0f);
    g_cube = cube();
    next_frame();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    CHECK(fake::pass3d_log().last_frame_stereo && fake::pass3d_log().stereo_changes == 1);
    hud_and_sky(); // 2D only: one picture (and the 3DS turns its 3D off)
    next_frame();
    CHECK(std::strcmp(last_calls(), "T") == 0);
    CHECK(!fake::pass3d_log().last_frame_stereo && fake::pass3d_log().stereo_changes == 2);
    next_frame();
    CHECK(fake::pass3d_log().stereo_changes == 2); // told every frame, changed once
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(stereo_without_the_capability_is_mono) {
    nnh::shutdown();
    fake::reset();
    fake::set_stereo_slider(1.0f); // a 2DS: no Stereo3D
    nnh::AppConfig config;
    config.gfx.max_draws_3d = 8;
    CHECK(nnh::init(config));
    g_cube = cube();
    next_frame();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    CHECK(std::strcmp(last_calls(), "[]") == 0 && nnh::gfx3d::stats().eyes == 1);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(stereo_culling_keeps_what_either_eye_sees) {
    restart_stereo(1.0f);
    g_cube = cube(0.1f);
    nnh::Camera3D cam;
    cam.stereo.strength = 4; // eyes at x = ∓2: at one unit ahead each sees x ≈ ±(0.64 … 2.56), the centre sees ±0.96
    nnh::gfx3d::set_camera(Screen::Top, cam);
    next_frame();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({2.3f, 0, 4}));  // only the right eye
    nnh::gfx3d::draw(Screen::Top, g_cube, at({-2.3f, 0, 4})); // only the left eye
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, 4}));     // between the eyes, seen by neither
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, -3}));    // both
    next_frame();
    CHECK(nnh::gfx3d::stats().culled == 1 && nnh::gfx3d::stats().triangles[0] == 36);
    fake::set_stereo_slider(0.0f);
    nnh::gfx3d::draw(Screen::Top, g_cube, at({2.3f, 0, 4}));
    nnh::gfx3d::draw(Screen::Top, g_cube, at({-2.3f, 0, 4}));
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, 4}));
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, -3}));
    next_frame();
    CHECK(nnh::gfx3d::stats().culled == 2 && nnh::gfx3d::stats().eyes == 1);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(stereo_in_view_is_the_engines_culling_test) {
    nnh::Camera3D cam;
    cam.stereo.strength = 4;
    const nnh::Sphere right_only{{2.3f, 0, 4}, 0.05f}, between{{0, 0, 4}, 0.05f};
    CHECK(!nnh::gfx3d::in_view(cam, right_only, 0.0f) && nnh::gfx3d::in_view(cam, right_only, 1.0f));
    CHECK(nnh::gfx3d::in_view(cam, between, 0.0f) && !nnh::gfx3d::in_view(cam, between, 1.0f));
    cam.stereo.enabled = false;
    CHECK(!nnh::gfx3d::in_view(cam, right_only, 1.0f));
}

NNH_TEST(stereo_eye_to_screen_puts_the_focal_plane_on_the_same_pixel) {
    nnh::Camera3D cam;
    const nnh::ScreenPoint l = nnh::gfx3d::to_screen(cam, {1, 0.5f, 0}, nnh::Eye::Left, 1.0f);
    const nnh::ScreenPoint r = nnh::gfx3d::to_screen(cam, {1, 0.5f, 0}, nnh::Eye::Right, 1.0f);
    const nnh::ScreenPoint c = nnh::gfx3d::to_screen(cam, {1, 0.5f, 0});
    CHECK(std::fabs(l.pixel.x - r.pixel.x) < 1e-3f && std::fabs(l.pixel.x - c.pixel.x) < 1e-3f && l.in_view);
    const nnh::ScreenPoint ln = nnh::gfx3d::to_screen(cam, {0, 0, 3}, nnh::Eye::Left, 1.0f);
    const nnh::ScreenPoint rn = nnh::gfx3d::to_screen(cam, {0, 0, 3}, nnh::Eye::Right, 1.0f);
    CHECK(ln.pixel.x > rn.pixel.x + 1.0f && ln.depth == 2.0f);
    const nnh::ScreenPoint mono = nnh::gfx3d::to_screen(cam, {0, 0, 3}, nnh::Eye::Right, 0.0f);
    const nnh::ScreenPoint centre = nnh::gfx3d::to_screen(cam, {0, 0, 3});
    CHECK(mono.pixel.x == centre.pixel.x && mono.pixel.y == centre.pixel.y && mono.depth == centre.depth &&
          mono.in_front == centre.in_front && mono.in_view == centre.in_view);
}

NNH_TEST(stereo_tile_budget_is_spent_once_for_both_eyes) {
    restart_stereo(1.0f, 3);
    g_cube = cube();
    auto loaded = nnh::load_texture("tile");
    CHECK(loaded.ok());
    nnh::Texture tile = loaded ? std::move(loaded.value()) : nnh::Texture{};
    next_frame();
    nnh::detail::TileBatchRecord t;
    static int source = 0;
    t.expand = [](const nnh::detail::TileExpandRequest& r) {
        nnh::detail::TileExpandResult out;
        for (int i = 0; i < 3; ++i) {
            if (out.written == r.capacity) {
                ++out.dropped;
                continue;
            }
            r.out[out.written++] = {static_cast<std::int16_t>(i * 16), 0, 16, 0, 0};
        }
        return out;
    };
    t.source = &source;
    t.texture_slot = nnh::TextureAccess::slot(tile);
    t.texture_generation = nnh::TextureAccess::generation(tile);
    t.tile_w = t.tile_h = 16;
    nnh::detail::gfx_record_tiles(Screen::Top, 1, t);
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    CHECK(std::strcmp(last_calls(), "Lt[]Rt[]L") == 0);
    CHECK(fake::state().last_frame_tiles.batch_count == 2 && fake::state().last_frame_tiles.quad_count == 6);
    CHECK(nnh::gfx::stats().dropped_tiles == 0 && nnh::gfx::stats().tiles_last_frame == 3);
    g_cube.reset();
    tile.reset();
    nnh::shutdown();
}
