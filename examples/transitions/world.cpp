#include "world.hpp"

#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

namespace {

// Tile ids of art/tiles.png (index + 1).
enum : std::uint16_t { Stone = 1, Tiled, Wall, Plant, Rug, Crate };

bool build_room(nnh::Tilemap& map, const nnh::Texture& tiles, int id) {
    auto created = nnh::Tilemap::create({.width = kRoomCellsW, .height = kRoomCellsH, .max_tile_layers = 2});
    if (!created) return false;
    map = std::move(created.value());
    if (!map.add_tileset({.texture = &tiles})) return false;
    const auto floor = map.add_tile_layer({.name = "floor"});
    const auto things = map.add_tile_layer({.name = "things"});
    if (!floor || !things) return false;
    map.fill(floor.value(), 0, 0, kRoomCellsW, kRoomCellsH, {id == 0 ? Stone : Tiled});
    map.fill(things.value(), 0, 0, kRoomCellsW, 1, {Wall});
    map.fill(things.value(), 0, kRoomCellsH - 1, kRoomCellsW, 1, {Wall});
    map.fill(things.value(), 0, 0, 1, kRoomCellsH, {Wall});
    map.fill(things.value(), kRoomCellsW - 1, 0, 1, kRoomCellsH, {Wall});
    if (id == 0) {
        map.fill(things.value(), 9, 5, 7, 5, {Rug});
        map.set_tile(things.value(), 2, 2, {Plant});
        map.set_tile(things.value(), 22, 2, {Plant});
    } else {
        for (int i = 0; i < 4; ++i) map.set_tile(things.value(), 4 + i * 5, 10, {Crate});
        map.set_tile(things.value(), 12, 3, {Plant});
    }
    return true;
}

} // namespace

bool World::build() {
    auto loaded = nnh::load_texture("art/tiles");
    if (!loaded) {
        NNH_LOG_ERROR(example, "art/tiles: %s", loaded.error().message);
        return false;
    }
    tiles = std::move(loaded.value());
    return build_room(rooms[0], tiles, 0) && build_room(rooms[1], tiles, 1);
}
