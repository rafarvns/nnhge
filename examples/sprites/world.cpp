#include "world.hpp"

#include <cstdio>

#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace {

constexpr float kWorldWidth = 2048;
constexpr float kGroundY = 208; // top of the grass
constexpr float kSpeed = 120;   // pixels per second
constexpr float kScale = 2;     // the 16x16 critter is drawn 32x32
constexpr int kTile = 32;

using nnh::Screen;

float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace

void World::update() {
    // One fixed step per frame: the engine runs at 60 Hz on every system, and a fixed step makes
    // screenshots reproducible (nnh::time::delta() would follow the real clock).
    constexpr float dt = 1.0f / 60;
    float move = nnh::input::circle_pad().x;
    if (nnh::input::held(nnh::Button::Left)) move = -1;
    if (nnh::input::held(nnh::Button::Right)) move = 1;
    walking_ = move != 0;
    if (walking_) {
        facing_left_ = move < 0;
        hero_.x = clamp(hero_.x + move * kSpeed * dt, 16, kWorldWidth - 16);
        walk_time_ += dt;
    } else {
        walk_time_ = 0;
    }
    ghost_angle_ += 1.5f * dt;

    // The camera keeps the critter a bit left of the middle, without leaving the world.
    const float camera_x = clamp(hero_.x - 160, 0, kWorldWidth - static_cast<float>(nnh::screen_width(Screen::Top)));
    nnh::gfx::set_camera(Screen::Top, {.position = {camera_x, 0}});
}

void World::draw() const {
    draw_sky();
    draw_ground();
    const int frame = walking_ ? static_cast<int>(walk_time_ * 8) % 4 : 0;
    nnh::gfx::draw(Screen::Top, art_.hero, hero_,
                   {.region = {frame * 16, 0, 16, 16},
                    .pivot = {8, 16},
                    .scale = {kScale, kScale},
                    .flip_x = facing_left_,
                    .layer = 3});
    draw_hud();
    draw_minimap();
}

void World::draw_sky() const {
    // Far clouds move at a third of the camera speed (parallax), so they are drawn camera-free.
    const float far = nnh::gfx::camera(Screen::Top).position.x / 3;
    const nnh::Color cloud = nnh::rgba(255, 255, 255, 200);
    for (int i = 0; i < 6; ++i) {
        const float x = static_cast<float>(i * 170) - far;
        const float y = static_cast<float>(30 + (i % 3) * 22);
        nnh::gfx::fill_rect(Screen::Top, {x, y, 56, 12}, cloud, {.ignore_camera = true});
        nnh::gfx::fill_rect(Screen::Top, {x + 10, y - 8, 30, 10}, cloud, {.ignore_camera = true});
    }
    // A ghost of the critter spinning in the sky: rotation around its center, tint and opacity.
    nnh::gfx::draw(Screen::Top, art_.hero, {300, 90},
                   {.region = {0, 0, 16, 16},
                    .pivot = {8, 8},
                    .rotation = ghost_angle_,
                    .scale = {3, 3},
                    .tint = nnh::rgb(200, 150, 255),
                    .opacity = 0.6f,
                    .layer = 2});
}

void World::draw_ground() const {
    // Only the tiles the camera sees.
    const int first = static_cast<int>(nnh::gfx::camera(Screen::Top).position.x) / kTile;
    for (int i = first; i <= first + nnh::screen_width(Screen::Top) / kTile + 1; ++i) {
        const float x = static_cast<float>(i * kTile);
        nnh::gfx::draw(Screen::Top, art_.tiles, {x, kGroundY}, {.region = {0, 0, kTile, kTile}, .layer = 1});
        // Floating platforms from the level file (assets/levels/level1.txt), repeating every 16 tiles.
        for (int p = 0; p < level_.count; ++p) {
            const Platform& platform = level_.platforms[p];
            if (i % 16 < platform.first || i % 16 > platform.last) continue;
            nnh::gfx::draw(Screen::Top, art_.tiles, {x, platform.y}, {.region = {0, 0, kTile, kTile}, .layer = 1});
            nnh::gfx::draw(Screen::Top, art_.tiles, {x, platform.y + kTile},
                           {.region = {kTile, 0, kTile, 16}, .layer = 1});
        }
    }
}

void World::draw_hud() const {
    // HUD: camera-free, on the top layers. The box is sized by measuring the text, which gives the same
    // size on every system (built-in font), so it fits on the console exactly as in the simulator.
    char text[32];
    std::snprintf(text, sizeof text, "x %4d / %d", static_cast<int>(hero_.x), static_cast<int>(kWorldWidth));
    const nnh::Vec2 size = nnh::gfx::measure_text(text);
    const nnh::Rect box{4, 4, size.x + 10, size.y + 6};
    const nnh::ShapeOptions hud{.layer = 14, .ignore_camera = true};
    nnh::gfx::fill_rect(Screen::Top, box, nnh::rgba(0, 0, 0, 150), hud);
    nnh::gfx::draw_rect(Screen::Top, box, nnh::colors::white, hud);
    nnh::gfx::draw_text(Screen::Top, {box.x + 5, box.y + 3}, text, {.layer = 15, .ignore_camera = true});
}

void World::draw_minimap() const {
    constexpr float kLeft = 20;
    constexpr float kWidth = 280;
    constexpr float kTop = 60;
    const float scale = kWidth / kWorldWidth;
    const float camera_x = nnh::gfx::camera(Screen::Top).position.x;
    const float view = static_cast<float>(nnh::screen_width(Screen::Top)) * scale;
    nnh::gfx::fill_rect(Screen::Bottom, {kLeft, kTop, kWidth, 40}, nnh::rgb(40, 80, 50));
    nnh::gfx::draw_line(Screen::Bottom, {kLeft, kTop + 30}, {kLeft + kWidth, kTop + 30}, nnh::rgb(120, 200, 100));
    nnh::gfx::draw_rect(Screen::Bottom, {kLeft + camera_x * scale, kTop, view, 40}, nnh::colors::yellow);
    nnh::gfx::fill_rect(Screen::Bottom, {kLeft + hero_.x * scale - 2, kTop + 24, 4, 6}, nnh::rgb(240, 160, 64));
    nnh::debug::print(Screen::Bottom, 20, 20, "D-Pad / Circle Pad: walk");
    nnh::debug::print(Screen::Bottom, 20, 34, "SELECT: benchmark   START: exit");
    const nnh::gfx::Stats s = nnh::gfx::stats();
    nnh::debug::printf(Screen::Bottom, 20, 120, "draws %d / %d, textures %d", s.draws_last_frame, s.max_draws,
                       s.textures);
}
