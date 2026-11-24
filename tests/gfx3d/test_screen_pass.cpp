// The 3D pass among the 2D layers (spec 026 FR-18/FR-19/FR-20/FR-25, T4.4–T4.7): 2D-only frames unchanged, 2D below
// the world layer → 3D → 2D above, world layers 0 and 16, culling, opaque in call order then blended back to front,
// the platform's draw (decode folded into the model-view), lights in view space.

#include <cstring>

#include "gfx/world_split.hpp"
#include "gfx3d/pass3d.hpp"
#include "gfx3d_fixture.hpp"

using namespace gfx3d_test;
using nnh::Screen;
using nnh::Vec3;

namespace {

nnh::Mesh g_cube;

const char* last_calls() { return fake::state().last_frame_calls; }

void hud_and_sky() {
    nnh::gfx::fill_rect(Screen::Top, {0, 0, 400, 240}, {10, 10, 80, 255}, {.layer = 2}); // sky
    nnh::gfx::fill_rect(Screen::Top, {4, 4, 20, 8}, {255, 255, 255, 255}, {.layer = 9});  // HUD
}

} // namespace

NNH_TEST(gfx3d_a_frame_without_3d_draws_submits_2d_as_before) {
    restart();
    next_frame();
    hud_and_sky();
    next_frame();
    CHECK(std::strcmp(last_calls(), "T") == 0); // one draw_2d, no 3D pass
    CHECK(fake::pass3d_log().last_frame_call_count == 0);
    nnh::shutdown();
}

NNH_TEST(gfx3d_the_world_sits_between_the_2d_layers) {
    restart();
    g_cube = cube();
    next_frame();
    hud_and_sky();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    CHECK(std::strcmp(last_calls(), "T[]T") == 0);
    const fake::Fake3D& l = fake::pass3d_log();
    CHECK(l.last_frame_call_count == 3 && l.last_frame_calls[0].pass.below_2d);
    CHECK(fake::state().last_frame_items[0].layer == 2 && fake::state().last_frame_items[1].layer == 9);
    nnh::gfx3d::set_world_layer(Screen::Top, 0); // everything 2D over the world
    hud_and_sky();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    CHECK(std::strcmp(last_calls(), "[]T") == 0 && !fake::pass3d_log().last_frame_calls[0].pass.below_2d);
    nnh::gfx3d::set_world_layer(Screen::Top, 16); // everything 2D under it
    hud_and_sky();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    CHECK(std::strcmp(last_calls(), "T[]") == 0);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(gfx3d_draws_outside_the_view_are_culled) {
    restart();
    g_cube = cube();
    next_frame();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, 0}));     // in view
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, 20}));    // behind the camera at z = 5
    nnh::gfx3d::draw(Screen::Top, g_cube, at({100, 0, 0}));   // far to the right
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, -200}));  // beyond the far plane (100)
    nnh::gfx3d::draw(Screen::Top, g_cube, at({4.2f, 0, 0}));  // straddling the right edge: kept
    next_frame();
    const nnh::platform::Draw3D* d[8];
    CHECK(last_draws(d, 8) == 2 && nnh::gfx3d::stats().culled == 3);
    CHECK(nnh::gfx3d::stats().triangles[0] == 24 && nnh::gfx3d::stats().eyes == 1);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(gfx3d_cull_planes) {
    nnh::Camera3D cam;
    const nnh::detail::Frustum f = nnh::detail::make_frustum(cam);
    CHECK(nnh::detail::sphere_visible(f, {0, 0, -5}, 0.5f));
    CHECK(!nnh::detail::sphere_visible(f, {0, 0, 1}, 0.5f));   // behind
    CHECK(nnh::detail::sphere_visible(f, {0, 0, 0.3f}, 0.5f)); // straddles the near plane
    CHECK(!nnh::detail::sphere_visible(f, {0, 3.1f, -5}, 0.1f)); // above: tan 30° × 5 = 2.89
    CHECK(nnh::detail::sphere_visible(f, {0, 3.1f, -5}, 0.2f));
    cam.projection = nnh::Projection::Orthographic; // height 10: half 5, half width 8.33
    const nnh::detail::Frustum o = nnh::detail::make_frustum(cam);
    CHECK(nnh::detail::sphere_visible(o, {8.3f, 0, -50}, 0.1f) && !nnh::detail::sphere_visible(o, {8.5f, 0, -50}, 0.1f));
}

NNH_TEST(gfx3d_order_opaque_in_call_order_then_blended_back_to_front) {
    const bool visible[6] = {true, true, true, false, true, true};
    const bool blended[6] = {false, true, false, true, true, true};
    const float depth[6] = {-1, -3, -2, -9, -7, -3};
    std::uint16_t order[12], scratch[6];
    const nnh::detail::Order o = nnh::detail::order_draws(visible, blended, depth, 6, order, scratch);
    CHECK(o.opaque == 5 && o.blended == 3);
    const std::uint16_t want[8] = {0, 1, 2, 4, 5, 4, 1, 5}; // blended: −7, then the −3 tie in call order
    CHECK(std::memcmp(order, want, sizeof want) == 0);
}

NNH_TEST(gfx3d_blended_draws_follow_opaque_ones) {
    restart();
    g_cube = cube();
    next_frame();
    const nnh::Material3D glass{.base_color = {255, 255, 255, 100}, .alpha = nnh::AlphaMode::Blend};
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, -1}), {.material = &glass}); // nearer glass
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, -4}), {.material = &glass}); // farther glass
    nnh::gfx3d::draw(Screen::Top, g_cube, at({1, 0, 0}));                       // opaque, last call
    next_frame();
    const nnh::platform::Draw3D* d[4];
    CHECK(last_draws(d, 4) == 3);
    CHECK(d[0]->material.alpha == 0 && d[1]->material.alpha == 2 && d[2]->material.alpha == 2);
    CHECK(d[1]->model_view[11] < d[2]->model_view[11]); // the farther glass first
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(gfx3d_platform_draw_folds_the_decode_into_the_model_view) {
    restart();
    g_cube = cube(2);
    next_frame();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({1, 0, 0}));
    next_frame();
    const nnh::platform::Draw3D* d[2];
    CHECK(last_draws(d, 2) == 1);
    // A cube corner stored as (32767, 32767, 32767) lands at (1 + 1, 1, 1) in the world, (2, 1, −4) in view space.
    const float q = 32767;
    const float* mv = d[0]->model_view;
    CHECK(std::fabs((mv[0] * q + mv[1] * q + mv[2] * q + mv[3]) - 2.0f) < 1e-4f);
    CHECK(std::fabs((mv[8] * q + mv[9] * q + mv[10] * q + mv[11]) + 4.0f) < 1e-4f);
    CHECK(d[0]->normal[0] == 1.0f && d[0]->normal[4] == 1.0f && d[0]->index_count == 36);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(gfx3d_lights_default_and_move_to_view_space) {
    restart();
    g_cube = cube();
    next_frame();
    nnh::Camera3D cam;
    cam.position = {10, 0, 0};
    nnh::gfx3d::point_at(cam, {0, 0, 0}); // looking down −x
    nnh::gfx3d::set_camera(Screen::Top, cam);
    nnh::Lighting l;
    l.count = 2;
    l.lights[0] = {.kind = nnh::LightKind::Directional, .vector = {1, 0, 0}};
    l.lights[1] = {.kind = nnh::LightKind::Point, .vector = {10, 0, -2}, .range = 4};
    nnh::gfx3d::set_lighting(Screen::Top, l);
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    const nnh::platform::Pass3D& p = fake::pass3d_log().last_frame_calls[0].pass;
    CHECK(p.light_count == 2 && p.lights[1].kind == 1 && p.lights[1].inv_range == 0.25f);
    CHECK(std::fabs(p.lights[0].vector[2] - 1) < 1e-6f);                       // towards the camera: +z in view
    CHECK(std::fabs(p.lights[1].vector[0] - 2) < 1e-5f && std::fabs(p.lights[1].vector[2]) < 1e-5f); // to the right
    CHECK(std::fabs(p.ambient[0] - 77.0f / 255) < 1e-6f);
    CHECK(nnh::gfx3d::lighting(Screen::Top).count == 2);
    CHECK(asserts_or_is_ignored([] {
        nnh::Lighting bad;
        bad.count = 5;
        nnh::gfx3d::set_lighting(Screen::Top, bad);
    }));
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(gfx3d_invalid_cameras_are_refused) {
    restart();
    nnh::Camera3D c;
    c.position = {1, 2, 3};
    nnh::gfx3d::set_camera(Screen::Top, c);
    CHECK(asserts_or_is_ignored([] {
        nnh::Camera3D bad;
        bad.near_plane = 0;
        nnh::gfx3d::set_camera(Screen::Top, bad);
    }));
    CHECK(nnh::gfx3d::camera(Screen::Top).position == (Vec3{1, 2, 3}));
    nnh::shutdown();
}

NNH_TEST(gfx3d_world_split_finds_the_first_item_and_batch_at_the_layer) {
    nnh::platform::DrawItem2D items[4];
    items[0].layer = 0, items[1].layer = 7, items[2].layer = 8, items[3].layer = 12;
    const nnh::detail::TileSplit splits[3] = {{1, 0, 3}, {2, 1, 8}, {4, 2, 15}};
    const auto s = nnh::detail::world_split(items, 4, splits, 3, 8);
    CHECK(s.item == 2 && s.split == 1);
    const auto all = nnh::detail::world_split(items, 4, splits, 3, 16);
    CHECK(all.item == 4 && all.split == 3);
}
