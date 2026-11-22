// Fuzz target body for `.nnhmap` maps (spec 012 AC-3; see fuzz_targets.hpp): validate, copy into a map with
// extra room, then run every kind of query, the draw-time tile expansion and a few edits on what was accepted.

#include "fuzz_targets.hpp"

#include <cstdlib>
#include <cstring>
#include <utility>

#include "core/crc32.hpp"
#include "nnh/tilemap.hpp"
#include "tilemap/map_format.hpp"
#include "tilemap/map_validate.hpp"
#include "tilemap/tile_expand.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace nnh::fuzz {

namespace {

using namespace detail::mapfmt;

/// Reads every property of `p` by its own name, through every getter.
void read_properties(const Properties& p) {
    for (int i = 0; i < p.count(); ++i) {
        const char* name = p.name(i);
        if (!p.has(name) || p.type(name) == nnh::PropertyType::None) std::abort(); // a name it lists must be found
        (void)p.get_bool(name);
        (void)p.get_int(name);
        (void)p.get_float(name);
        if (std::strlen(p.get_string(name)) > kMaxStringLength) std::abort();
    }
}

/// Every layer × tileset expanded for a 400 × 240 screen, as at the end of a frame.
void expand_all(const Tilemap& map) {
    static platform::TileQuad2D quads[1600];
    const detail::MapView v = detail::map_view(map);
    const Header& h = v.header();
    for (int layer = 0; layer < h.tile_layers; ++layer) {
        const TileLayerEntry& e = v.tile_layers()[layer];
        detail::LayerPlacement p;
        p.camera = {static_cast<float>(h.tile_width), static_cast<float>(h.tile_height)};
        p.screen_size = {400, 240};
        p.offset = {e.offset_x, e.offset_y};
        p.parallax = {e.parallax_x, e.parallax_y};
        p.parallax_origin = {h.parallax_origin_x, h.parallax_origin_y};
        for (int s = 0; s < h.tilesets; ++s) {
            detail::ExpandInput in;
            in.cells = v.cells(layer);
            in.map_width = h.width;
            in.sources = v.tile_sources();
            in.first_id = v.tilesets()[s].first_id;
            in.tile_count = v.tilesets()[s].tile_count;
            in.view = detail::visible_range(p, h.tile_width, h.tile_height, h.width, h.height);
            in.tile_w = h.tile_width;
            in.tile_h = h.tile_height;
            const detail::ExpandResult r = detail::expand_tiles(in, quads, 1600);
            if (r.written < 0 || r.written > 1600) std::abort();
        }
    }
}

void query_everything(Tilemap& map) {
    for (int layer = 0; layer < map.tile_layer_count(); ++layer) {
        (void)map.find_tile_layer(map.tile_layer_name(layer));
        read_properties(map.properties(PropertyOwner::tile_layer(layer)));
        for (int y = 0; y < map.height(); ++y) {
            for (int x = 0; x < map.width(); ++x) {
                if (map.tile(layer, x, y).id > map.tile_id_count()) std::abort();
                if (static_cast<int>(map.collision_at(x, y)) > 2) std::abort();
            }
        }
    }
    for (int id = 1; id <= map.tile_id_count(); ++id) read_properties(map.properties(PropertyOwner::tile(id)));
    for (int i = 0; i < map.object_layer_count(); ++i) read_properties(map.properties(PropertyOwner::object_layer(i)));
    for (int i = 0; i < map.object_count(); ++i) {
        const MapObject o = map.object(i);
        if (o.object_layer < 0 || o.object_layer >= map.object_layer_count()) std::abort();
        if (map.find_object(o.name) > i) std::abort(); // the first object with that name is at most this one
        read_properties(o.properties);
    }
    read_properties(map.properties());
    const collision::MoveResult moved = collision::move(map.collision_grid(), {1, 1, 10, 14}, {300, 200});
    (void)moved;
    expand_all(map);
}

/// Edits into the extra room, then the queries again.
void edit(Tilemap& map) {
    const auto layer = map.add_tile_layer({.name = "fuzz", .collision = true});
    if (!layer) std::abort(); // one extra layer was asked for and the limits allow 16
    map.fill(layer.value(), 0, 0, map.width(), 1, Tile{static_cast<std::uint16_t>(map.tile_id_count())});
    if (map.tile_id_count() > 0) map.set_tile_collision(1, collision::Cell::OneWay);
    (void)map.set_int(PropertyOwner::map(), "fuzz", 1);
    if (map.object_layer_count() > 0) (void)map.add_object(0, {.name = "fuzz", .bounds = {1, 2, 3, 4}});
    query_everything(map);
}

} // namespace

int tilemap_target(const std::uint8_t* data, std::size_t size) {
    // As is (the header and CRC paths), then with the size and CRC fixed so the content checks run.
    (void)detail::validate_map(data, size);
    static std::uint8_t file[256 * 1024];
    if (size < sizeof(Header) || size > sizeof file) return 0;
    std::memcpy(file, data, size);
    Header h;
    std::memcpy(&h, file, sizeof h);
    h.file_size = static_cast<std::uint32_t>(size);
    h.crc = detail::crc32(file + 16, size - 16);
    std::memcpy(file, &h, sizeof h);
    if (!detail::validate_map(file, size).ok()) return 0;
    auto made = detail::tilemap_from_file(file, size, {.extra_tile_layers = 1, .extra_objects = 1,
                                                       .extra_properties = 2, .extra_string_bytes = 16},
                                          "fuzz");
    if (!made) return 0; // the extras can push a map over a limit
    Tilemap map = std::move(made.value());
    query_everything(map);
    edit(map);
    return 0;
}

} // namespace nnh::fuzz
