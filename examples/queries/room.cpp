#include "room.hpp"

#include <nnh/log.hpp>
#include <utility>

NNH_LOG_CHANNEL_EXTERN(example);

namespace room {

namespace {

// '#' wall, '~' water, '=' plank, '.' floor.
const char* const kLayout[kHeight] = {
    "#########################", //
    "#.......................#", //
    "#.......................#", //
    "#......####.............#", //
    "#......#................#", //
    "#......#.......=====....#", //
    "#.......................#", //
    "#...........#...........#", //
    "#...........#...........#", //
    "#.....~~~~~~#..........##", //
    "#.....~~~~~~#...........#", //
    "#...===.........######..#", //
    "#.......................#", //
    "#.......................#", //
    "#########################", //
};

using namespace bits;

constexpr Thing kStartThings[kThings] = {
    {{248, 40, 16, 24}, kRed},   {{168, 120, 12, 12}, kRed},  {{312, 136, 20, 12}, kRed},
    {{56, 200, 16, 16}, kRed},   {{280, 56, 12, 20}, kGreen}, {{216, 104, 16, 16}, kGreen},
    {{104, 112, 12, 12}, kGreen}, {{336, 196, 24, 12}, kGreen}, {{136, 72, 20, 12}, kBlue},
    {{232, 200, 16, 16}, kBlue}, {{352, 88, 12, 24}, kBlue},  {{176, 208, 12, 12}, kBlue},
};

bool name_tiles(nnh::Tilemap& map) {
    const char* names[4] = {"wall", "water", "plank", "floor"};
    for (int id = Wall; id <= Floor; ++id) {
        if (!map.set_string(nnh::PropertyOwner::tile(id), "name", names[id - 1])) return false;
    }
    return true;
}

} // namespace

bool Room::build() {
    auto loaded = nnh::load_texture("art/tiles");
    if (!loaded) {
        NNH_LOG_ERROR(example, "art/tiles: %s", loaded.error().message);
        return false;
    }
    tiles = std::move(loaded.value());
    auto created = nnh::Tilemap::create({.width = kWidth, .height = kHeight, .max_tilesets = 1, .max_tile_ids = 4,
                                         .max_object_layers = 0, .max_objects = 0, .max_properties = 8});
    if (!created) return false;
    map = std::move(created.value());
    if (!map.add_tileset({.texture = &tiles, .name = "tiles"})) return false;
    const auto f = map.add_tile_layer({.name = "floor"});
    const auto w = map.add_tile_layer({.name = "walls", .collision = true}); // on kWorld (the default layer)
    const auto p = map.add_tile_layer({.name = "water", .collision = true, .collision_layers = kWater});
    const auto k = map.add_tile_layer({.name = "planks"});
    if (!f || !w || !p || !k) return false;
    floor = f.value();
    walls = w.value();
    water = p.value();
    planks = k.value();
    map.set_tile_collision(Plank, nnh::collision::Cell::OneWay); // one-way on kWorld
    map.fill(floor, 0, 0, kWidth, kHeight, {Floor});
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const char c = kLayout[y][x];
            if (c == '#') map.set_tile(walls, x, y, {Wall});
            if (c == '~') map.set_tile(water, x, y, {Water});
            if (c == '=') map.set_tile(planks, x, y, {Plank});
        }
    }
    for (int i = 0; i < kThings; ++i) things[i] = kStartThings[i];
    return name_tiles(map);
}

nnh::collision::BoxView Room::view() const {
    return nnh::collision::box_view(things, kThings, &Thing::box, &Thing::layers);
}

nnh::collision::Grid Room::grid(nnh::collision::Layers mask) const {
    nnh::collision::Grid g = map.collision_grid(mask & (kWorld | kWater)); // at most 4 masks: the map holds 8
    g.outside = nnh::collision::Cell::Solid;
    return g;
}

nnh::Color thing_color(nnh::collision::Layers layers) {
    if (layers == kRed) return nnh::rgb(230, 70, 70);
    if (layers == kGreen) return nnh::rgb(80, 200, 100);
    return nnh::rgb(90, 140, 255);
}

const char* thing_name(nnh::collision::Layers layers) {
    if (layers == kRed) return "red";
    if (layers == kGreen) return "green";
    return "blue";
}

} // namespace room
