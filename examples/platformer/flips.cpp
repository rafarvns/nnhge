#include "flips.hpp"

#include <nnh/gfx.hpp>
#include <utility>

namespace {

constexpr nnh::Screen kTop = nnh::Screen::Top;
constexpr const char* kLabels[8] = {"-", "x", "y", "xy", "d", "dx", "dy", "dxy"};

/// Orientation i: bit 0 flip x, bit 1 flip y, bit 2 diagonal (applied first, as in Tiled).
nnh::Tile arrow(int i) {
    return {.id = tile::Arrow, .flip_x = (i & 1) != 0, .flip_y = (i & 2) != 0, .flip_diagonal = (i & 4) != 0};
}

} // namespace

Flips::Flips(const Art& art) : art_(art) {
    auto made = nnh::Tilemap::create({.width = 40,
                                      .height = 15,
                                      .max_tile_layers = 2,
                                      .max_tilesets = 1,
                                      .max_tile_ids = tile::kCount,
                                      .max_object_layers = 0,
                                      .max_objects = 0,
                                      .max_properties = 0,
                                      .string_bytes = 64});
    if (made) map_ = std::move(made.value());
    if (!map_.valid() || !map_.add_tileset({.texture = &art.tiles, .tile_count = tile::kCount})) {
        map_.reset();
        return;
    }
    arrows_ = map_.add_tile_layer({.name = "arrows"}).value();
    wall_ = map_.add_tile_layer({.name = "wall"}).value();
    for (int i = 0; i < 8; ++i)
        map_.set_tile(arrows_, 1 + i * 3, 2, arrow(i));
    map_.fill(wall_, 0, 10, 40, 4, {.id = tile::Bricks});
    for (int x = 0; x < 40; x += 2)
        map_.set_tile(wall_, x, 9, {.id = tile::Plank, .flip_y = x % 4 == 0});
}

void Flips::update() {
    scroll_ += 0.37f; // fractions of a pixel: the wall's position is rounded once per layer, never per tile
    if (scroll_ > 200) scroll_ = 0;
    nnh::gfx::set_camera(kTop, {{scroll_, 0}});
}

void Flips::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_tilemap(kTop, map_, {.map_layer = arrows_, .ignore_camera = true});
    gfx::draw_tilemap(kTop, map_, {.map_layer = wall_});
    const nnh::TextOptions label{
        .color = nnh::colors::black, .align = nnh::TextAlign::Center, .ignore_camera = true};
    gfx::draw_text(kTop, {8, 8}, "map tiles: Tiled's 8 orientations",
                   {.color = nnh::colors::black, .ignore_camera = true});
    for (int i = 0; i < 8; ++i)
        gfx::draw_text(kTop, {24.0f + i * 48, 52}, kLabels[i], label);
    gfx::draw_text(kTop, {8, 72}, "sprites: flip_x / flip_y",
                   {.color = nnh::colors::black, .ignore_camera = true});
    for (int i = 0; i < 4; ++i) {
        gfx::draw(kTop, art_.tiles, {16.0f + i * 48, 88},
                  {.region = tile::region(tile::Arrow),
                   .flip_x = (i & 1) != 0,
                   .flip_y = (i & 2) != 0,
                   .ignore_camera = true});
        gfx::draw_text(kTop, {24.0f + i * 48, 108}, kLabels[i], label);
    }
    gfx::draw_text(kTop, {8, 128}, "wall: camera moves 0.37 px a frame",
                   {.color = nnh::colors::black, .ignore_camera = true});
    const nnh::Screen bottom = nnh::Screen::Bottom;
    gfx::draw_text(bottom, {12, 12}, "FLIP TEST",
                   {.color = nnh::rgb(250, 220, 120), .scale = 2, .shadow = true});
    gfx::draw_text(bottom, {12, 40},
                   "Top row: one tile in Tiled's 8\norientations (d = diagonal, applied\nfirst; then x, y).\n"
                   "The arrow and the yellow dot must\nmatch the PC screenshot.\n\n"
                   "Middle row: the same as sprites\n(they have no diagonal flip).\n\n"
                   "Wall: no gaps between tiles.\n\nSELECT: next demo   START: exit",
                   {.color = nnh::rgb(200, 210, 230)});
}
