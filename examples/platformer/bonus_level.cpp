#include "bonus_level.hpp"

#include "art.hpp"

#include <nnh/collision.hpp>
#include <utility>

namespace {

constexpr int kWidth = 100; // cells: 1600 px
constexpr int kHeight = 15; // cells: 240 px, the screen's height

struct Span {
    int from, to, row; // cells [from, to], one row
};

// Ground columns from `row` down to the bottom; row 0 = a pit.
constexpr Span kGround[] = {{0, 19, 12}, {20, 22, 0},  {23, 39, 11}, {40, 44, 12},
                            {45, 47, 0}, {48, 69, 12}, {70, 74, 10}, {75, 99, 12}};
constexpr Span kBricks[] = {{8, 11, 8}, {28, 31, 7}, {55, 60, 8}, {85, 88, 7}};
constexpr Span kPlanks[] = {{14, 17, 9}, {34, 37, 7}, {62, 66, 6}, {78, 82, 8}};
constexpr Span kClouds[] = {{2, 4, 2}, {10, 13, 1}, {18, 19, 3}, {26, 28, 2}, {34, 36, 1}, {40, 41, 3}};
constexpr Span kHills[] = {{0, 8, 3},   {6, 14, 5},  {13, 20, 2}, {19, 30, 4},
                           {28, 36, 6}, {35, 44, 3}, {42, 52, 5}, {50, 62, 4}}; // row = height in cells
constexpr int kBushes[] = {3, 25, 42, 52, 77, 93};
constexpr int kFlowers[] = {6, 17, 33, 50, 58, 90, 96};

nnh::Tile t(int id) {
    return {static_cast<std::uint16_t>(id)};
}

int ground_row(int x) {
    for (const Span& s : kGround) {
        if (x >= s.from && x <= s.to) return s.row;
    }
    return 0;
}

void build_ground(nnh::Tilemap& map, int ground, int front) {
    for (const Span& s : kGround) {
        if (s.row == 0) { // a pit with water at the bottom, drawn over the player
            map.fill(front, s.from, kHeight - 1, s.to - s.from + 1, 1, t(tile::Water));
            continue;
        }
        const int top = s.from >= 70 && s.to <= 74 ? tile::Stone : tile::Grass;
        const int below = top == tile::Stone ? tile::Stone : tile::Dirt;
        map.fill(ground, s.from, s.row, s.to - s.from + 1, 1, t(top));
        map.fill(ground, s.from, s.row + 1, s.to - s.from + 1, kHeight - s.row - 1, t(below));
    }
    for (const Span& s : kBricks)
        map.fill(ground, s.from, s.row, s.to - s.from + 1, 1, t(tile::Bricks));
    for (const Span& s : kPlanks)
        map.fill(ground, s.from, s.row, s.to - s.from + 1, 1, t(tile::Plank));
    for (int x : kFlowers)
        map.set_tile(ground, x, ground_row(x) - 1, t(tile::Flower)); // not marked: no collision
    for (int x : kBushes)
        map.set_tile(front, x, ground_row(x) - 1, t(tile::Bush));
    map.set_tile(front, 11, ground_row(11) - 1, {.id = tile::Tuft, .flip_x = true});
}

void build_backdrops(nnh::Tilemap& map, int clouds, int hills) {
    for (const Span& s : kClouds) {
        map.set_tile(clouds, s.from - 1, s.row, t(tile::CloudLeft));
        map.fill(clouds, s.from, s.row, s.to - s.from + 1, 1, t(tile::CloudMiddle));
        map.set_tile(clouds, s.to + 1, s.row, t(tile::CloudRight));
    }
    for (int x = 0; x < kWidth; ++x) {
        int height = 0;
        for (const Span& s : kHills) {
            if (x >= s.from && x <= s.to && s.row > height) height = s.row;
        }
        if (height == 0) continue;
        map.set_tile(hills, x, kHeight - height, t(tile::HillTop));
        map.fill(hills, x, kHeight - height + 1, 1, height - 1, t(tile::Hill));
    }
}

/// Coins above every brick and plank, as objects with a "value" property.
nnh::Result<void> add_coins(nnh::Tilemap& map, int things) {
    for (const Span* list : {kBricks, kPlanks}) {
        for (int i = 0; i < 4; ++i) {
            const Span& s = list[i];
            for (int x = s.from; x <= s.to; ++x) {
                const nnh::Rect bounds{x * 16.0f + 2, (s.row - 1) * 16.0f + 2, 12, 12};
                auto coin = map.add_object(things, {.name = "coin", .type = "coin", .bounds = bounds});
                if (!coin) return coin.error();
                if (auto set = map.set_int(nnh::PropertyOwner::object(coin.value()), "value", 1); !set)
                    return set;
            }
        }
    }
    return {};
}

} // namespace

nnh::Result<nnh::Tilemap> build_bonus_level(const nnh::Texture& tiles) {
    auto made = nnh::Tilemap::create({.width = kWidth,
                                      .height = kHeight,
                                      .max_tile_layers = 4,
                                      .max_tilesets = 1,
                                      .max_tile_ids = tile::kCount,
                                      .max_objects = 64,
                                      .string_bytes = 256,
                                      .max_animations = 1,
                                      .max_animation_frames = 2});
    if (!made) return made.error();
    nnh::Tilemap map = std::move(made.value());
    auto first = map.add_tileset({.texture = &tiles, .tile_count = tile::kCount, .name = "tiles"});
    if (!first) return first.error();
    int layers[4] = {};
    const nnh::TileLayerDesc descs[4] = {{.name = kBonusLayers[0], .parallax = {0.25f, 1}},
                                         {.name = kBonusLayers[1], .parallax = {0.5f, 1}},
                                         {.name = kBonusLayers[2]},
                                         {.name = kBonusLayers[3], .opacity = 0.85f}};
    for (int i = 0; i < 4; ++i) {
        auto added = map.add_tile_layer(descs[i]);
        if (!added) return added.error();
        layers[i] = added.value();
    }
    // Parallax layers line up with the map when the view's centre is here: the first screen.
    map.set_parallax_origin({200, 120});
    for (int id : {tile::Grass, tile::Dirt, tile::Bricks, tile::Stone})
        map.set_tile_collision(id, nnh::collision::Cell::Solid);
    map.set_tile_collision(tile::Plank, nnh::collision::Cell::OneWay);
    // The water in the pits ripples: every Water cell shows Water and Water2 in turn (Tiled: a tile animation).
    const nnh::Tilemap::AnimationFrame ripple[] = {{.tile_id = tile::Water, .ms = 400},
                                                   {.tile_id = tile::Water2, .ms = 400}};
    if (auto animated = map.set_tile_animation(tile::Water, ripple, 2); !animated) return animated.error();
    build_backdrops(map, layers[0], layers[1]);
    build_ground(map, layers[2], layers[3]);

    auto things = map.add_object_layer("things");
    if (!things) return things.error();
    auto spawn = map.add_object(things.value(),
                                {.name = "spawn", .shape = nnh::ObjectShape::Point, .bounds = {40, 192}});
    if (!spawn) return spawn.error();
    if (auto coins = add_coins(map, things.value()); !coins) return coins.error();
    return map;
}
