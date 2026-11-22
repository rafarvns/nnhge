#include "world.hpp"

#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

namespace world {
namespace {

// Tile ids of art/tiles.png (index + 1).
enum : std::uint16_t { FloorA = 1, FloorB, Wall, Door, FloorC, FloorD, Crate, Arrow };

constexpr std::uint16_t kFloors[4] = {FloorA, FloorB, FloorC, FloorD};

void build_room(nnh::Tilemap& map, int ground, int things, int rx, int ry) {
    const int x0 = rx * kRoomW, y0 = ry * kRoomH;
    map.fill(ground, x0, y0, kRoomW, kRoomH, {kFloors[ry * 2 + rx]});
    // Walls round the room, with a door in the middle of each inner wall.
    map.fill(things, x0, y0, kRoomW, 1, {Wall});
    map.fill(things, x0, y0 + kRoomH - 1, kRoomW, 1, {Wall});
    map.fill(things, x0, y0, 1, kRoomH, {Wall});
    map.fill(things, x0 + kRoomW - 1, y0, 1, kRoomH, {Wall});
    map.fill(things, x0 + kRoomW / 2 - 2, y0 + (ry == 0 ? kRoomH - 1 : 0), 4, 1, {Door});
    map.fill(things, x0 + (rx == 0 ? kRoomW - 1 : 0), y0 + kRoomH / 2 - 2, 1, 4, {Door});
    // A few crates and arrows (one in each of the eight orientations).
    for (int i = 0; i < 8; ++i) {
        map.set_tile(things, x0 + 6 + i * 3, y0 + 6, {Crate});
        map.set_tile(things, x0 + 6 + i * 3, y0 + 17,
                     {Arrow, (i & 4) != 0, (i & 2) != 0, (i & 1) != 0}); // flip x, flip y, diagonal
    }
}

/// One axis of a back-and-forth path of `length` pixels from `from`, `t` pixels along.
float ping_pong(float from, float length, int t) {
    const int period = static_cast<int>(2 * length);
    const int k = t % period;
    return from + static_cast<float>(k < length ? k : period - k);
}

} // namespace

bool World::build() {
    auto loaded = nnh::load_texture("art/tiles");
    if (!loaded) {
        NNH_LOG_ERROR(example, "art/tiles: %s", loaded.error().message);
        return false;
    }
    tiles = std::move(loaded.value());
    auto created = nnh::Tilemap::create({.width = kCellsW, .height = kCellsH, .max_tile_layers = 2});
    if (!created) return false;
    map = std::move(created.value());
    if (!map.add_tileset({.texture = &tiles})) return false;
    const auto ground = map.add_tile_layer({.name = "ground"});
    const auto things = map.add_tile_layer({.name = "things"});
    if (!ground || !things) return false;
    for (int ry = 0; ry < 2; ++ry)
        for (int rx = 0; rx < 2; ++rx)
            build_room(map, ground.value(), things.value(), rx, ry);
    return true;
}

void Square::move(nnh::Vec2 direction, nnh::Rect bounds) {
    position += direction * 2.0f;
    const float max_x = bounds.x + bounds.w - kSquare, max_y = bounds.y + bounds.h - kSquare;
    position.x = position.x < bounds.x ? bounds.x : (position.x > max_x ? max_x : position.x);
    position.y = position.y < bounds.y ? bounds.y : (position.y > max_y ? max_y : position.y);
}

nnh::Vec2 auto_target(int frame) {
    // A loop through the four rooms' doors: x and y bounce at different periods.
    return {ping_pong(100, 1040, frame * 2), ping_pong(120, 520, frame)};
}

} // namespace world
