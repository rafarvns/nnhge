// The showcase's drawing (showcase_scene.hpp): the 3D world, the sky gradient below the world layer, the HUD and the
// name tag above it, the bottom screen's buttons.

#include <cstdio>
#include <nnh/gfx.hpp>
#include <nnh/system.hpp>
#include <nnh/time.hpp>
#include <nnh/transform.hpp>

#include "showcase_scene.hpp"

namespace {

nnh::Transform3D upright(nnh::Vec3 at) { // a plane (facing +y) turned to face +z
    return {.position = at, .rotation = nnh::quat_axis_angle({1, 0, 0}, nnh::kQuarterTurn)};
}

} // namespace

void ShowcaseScene::draw_world() {
    using nnh::Screen;
    const World& w = world_;
    nnh::Lighting l;
    l.ambient = lighting_ ? nnh::Color{60, 60, 70, 255} : nnh::Color{255, 255, 255, 255};
    l.count = lighting_ ? (point_light_ ? 2 : 1) : 0;
    l.lights[0] = {.kind = nnh::LightKind::Directional, .vector = {-0.5f, 1, 0.7f}, .color = {235, 230, 215, 255}};
    const nnh::SinCos orbit = nnh::sin_cos(static_cast<float>(frame_) * 0.015f);
    l.lights[1] = {.kind = nnh::LightKind::Point, .vector = {orbit.cos * 3, 1.6f, orbit.sin * 3},
                   .color = {255, 150, 60, 255}, .range = 6};
    nnh::gfx3d::set_lighting(Screen::Top, l);

    const nnh::Material3D plain{.base_color = {190, 190, 200, 255}};
    const nnh::Material3D* plain_or_null = textures_ ? nullptr : &plain;
    nnh::gfx3d::draw(Screen::Top, w.floor, nnh::Transform3D{}, {.material = plain_or_null});
    const float spin = static_cast<float>(frame_) * 0.02f;
    const nnh::Color tint = flash_ > 0 ? nnh::Color{255, 90, 90, 255} : nnh::colors::white;
    nnh::gfx3d::draw(Screen::Top, w.ship,
                     nnh::Transform3D{.position = {0, 1.0f, 0}, .rotation = nnh::quat_euler(spin, 0.15f, 0),
                                      .scale = {1.5f, 1.5f, 1.5f}},
                     {.material = plain_or_null, .tint = tint});
    nnh::gfx3d::draw(Screen::Top, w.pylon, nnh::Transform3D{.position = {-2.6f, 0, -1.2f}});
    nnh::Material3D cube_m = w.cube.material(0);
    cube_m.cull_back = culling_;
    nnh::gfx3d::draw(Screen::Top, w.cube,
                     nnh::Transform3D{.position = {2.2f, 0.5f, -1}, .rotation = nnh::quat_axis_angle({0, 1, 0}, -spin)},
                     {.material = &cube_m});
    nnh::gfx3d::draw(Screen::Top, w.sphere, nnh::Transform3D{.position = {-1.6f, 0.6f, 1.6f}});
    nnh::gfx3d::draw(Screen::Top, w.fence, upright({0, 0.5f, -3}));
    nnh::gfx3d::draw(Screen::Top, w.glass, upright({-1.0f, 0.9f, 2.6f}));
}

void ShowcaseScene::draw_sky_and_hud() {
    using nnh::Screen;
    for (int band = 0; band < 12; ++band) { // the sky: layer 0, under the 3D world
        const auto k = static_cast<std::uint8_t>(band * 12);
        nnh::gfx::fill_rect(Screen::Top, {0, static_cast<float>(band * 20), 400, 20},
                            {static_cast<std::uint8_t>(40 + k), static_cast<std::uint8_t>(70 + k), 160, 255},
                            {.layer = 0, .ignore_camera = true});
    }
    char text[96];
    const nnh::Stats3D s = nnh::gfx3d::stats();
    std::snprintf(text, sizeof text, "NNHGE models  %d draws %d culled %d tris", s.draws, s.culled, s.triangles[0]);
    nnh::gfx::draw_text(Screen::Top, {4, 4}, text, {.layer = 9});
    const nnh::ScreenPoint tag = nnh::gfx3d::to_screen(camera(), {0, 1.9f, 0});
    if (tag.in_view) nnh::gfx::draw_text(Screen::Top, {tag.pixel.x, tag.pixel.y - 8}, "ship",
                                         {.align = nnh::TextAlign::Center, .shadow = true, .layer = 10});
}

void ShowcaseScene::draw_bottom() {
    using nnh::Screen;
    const char* names[6] = {"texture", "lighting", "point light", "culling", "tint", "debug draw"};
    const bool states[6] = {textures_, lighting_, point_light_, culling_, flash_ > 0, debug_};
    const nnh::Rect rects[6] = {{10, 40, 95, 50}, {115, 40, 95, 50}, {220, 40, 95, 50},
                                {10, 110, 95, 50}, {115, 110, 95, 50}, {10, 180, 95, 50}};
    nnh::gfx::draw_text(Screen::Bottom, {8, 8}, "3D foundation (spec 026)", {});
    for (int i = 0; i < 6; ++i) {
        nnh::gfx::fill_rect(Screen::Bottom, rects[i], states[i] ? nnh::Color{60, 140, 80, 255} : nnh::Color{70, 70, 90, 255});
        nnh::gfx::draw_text(Screen::Bottom, {rects[i].x + 6, rects[i].y + 20}, names[i], {.layer = 1});
    }
    char text[64];
    std::snprintf(text, sizeof text, "%s  X: projection\nSELECT check  Y bench", ortho_ ? "ortho" : "persp");
    nnh::gfx::draw_text(Screen::Bottom, {115, 184}, text, {});
}
