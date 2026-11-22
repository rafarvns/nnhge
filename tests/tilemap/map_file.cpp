// Test-only `.nnhmap` writer (see map_file.hpp).

#include "map_file.hpp"

#include <cstring>

#include "core/crc32.hpp"
#include "tilemap/map_property_table.hpp"
#include "tilemap/map_strings.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace map_file {

namespace {

using namespace nnh::detail;
using namespace nnh::detail::mapfmt;

std::uint16_t str(const MapView& v, const char* text) { return static_cast<std::uint16_t>(map_intern(v, text)); }

PropertyEntry& add_property(const MapView& v, PropertyRange range, const char* name, PropertyType type) {
    const int index = insert_property(v, range);
    PropertyEntry& p = v.properties()[index];
    p.name = str(v, name);
    p.type = static_cast<std::uint8_t>(type);
    return p;
}

void add_tileset(const MapView& v, const char* image) {
    Header& h = v.header();
    TilesetEntry& s = v.tilesets()[0];
    s.first_id = 1;
    s.tile_count = 8;
    s.columns = 4;
    s.image_width = 64;
    s.image_height = 32;
    s.name = str(v, image);
    s.texture_slot = -1;
    for (int i = 0; i < 8; ++i) {
        v.tile_sources()[1 + i] = {static_cast<std::uint16_t>(i % 4 * 16), static_cast<std::uint16_t>(i / 4 * 16)};
    }
    h.tilesets = 1;
    h.tile_ids = 8;
    v.tile_flags()[3] = 1; // collision::Cell::Solid
    v.tile_flags()[5] = 2; // collision::Cell::OneWay
    const int entry = add_tile_props(v, 3);
    TilePropsEntry& t = v.tile_props()[entry];
    add_property(v, {&t.properties_first, &t.properties_count}, "damage", PropertyType::Int).value = 2;
}

void add_layers(const MapView& v) {
    Header& h = v.header();
    TileLayerEntry& back = v.tile_layers()[0];
    back.name = str(v, "back");
    back.flags = kLayerVisible;
    back.tilesets_used = 1;
    back.parallax_x = 0.5f;
    back.parallax_y = 1;
    back.opacity = 0.75f;
    TileLayerEntry& ground = v.tile_layers()[1];
    ground.name = str(v, "ground");
    ground.flags = kLayerVisible | kLayerCollision;
    ground.tilesets_used = 1;
    ground.parallax_x = ground.parallax_y = ground.opacity = 1;
    h.tile_layers = 2;
    std::uint16_t* cells = v.cells(0);
    cells[0] = encode_cell(1, false, false, false);
    cells[1] = encode_cell(2, true, false, false);
    cells[8 + 2] = encode_cell(5, false, false, true);
    cells[8 + 3] = encode_cell(4, false, true, false);
    for (int x = 0; x < 8; ++x) v.cells(1)[3 * 8 + x] = encode_cell(3, false, false, false);
    const float depth = 1.5f;
    PropertyEntry& p = add_property(v, {&back.properties_first, &back.properties_count}, "depth", PropertyType::Float);
    std::memcpy(&p.value, &depth, sizeof depth);
}

void add_objects(const MapView& v) {
    Header& h = v.header();
    ObjectLayerEntry& layer = v.object_layers()[0];
    layer.name = str(v, "things");
    layer.object_count = 2;
    h.object_layers = 1;
    add_property(v, {&layer.properties_first, &layer.properties_count}, "active", PropertyType::Bool).value = 1;
    ObjectEntry& spawn = v.objects()[0];
    spawn.id = 7;
    spawn.name = spawn.type = str(v, "spawn");
    spawn.shape = static_cast<std::uint8_t>(ObjectShape::Point);
    spawn.x = 16;
    spawn.y = 32;
    ObjectEntry& coin = v.objects()[1];
    coin.id = 9;
    coin.name = coin.type = str(v, "coin");
    coin.shape = static_cast<std::uint8_t>(ObjectShape::Rect);
    coin.x = 48;
    coin.y = coin.w = coin.h = 16;
    h.objects = 2;
    add_property(v, {&coin.properties_first, &coin.properties_count}, "value", PropertyType::Int).value = 5;
}

void add_animations(const MapView& v) {
    auto* animations = v.section<AnimationEntry>(kAnimations);
    auto* frames = v.section<FrameEntry>(kFrames);
    animations[0] = {.tile_id = 2, .frame_count = 2, .first_frame = 0};
    animations[1] = {.tile_id = 6, .frame_count = 1, .first_frame = 2};
    frames[0] = {2, 100};
    frames[1] = {4, 50};
    frames[2] = {7, 30};
    v.tile_flags()[2] |= kTileAnimated;
    v.tile_flags()[6] |= kTileAnimated;
    v.header().animations = 2;
    v.header().frames = 3;
}

Bytes sample_map(const char* image, bool animated) {
    const MapCapacities caps{.width = 8, .height = 4, .tilesets = 1, .tile_ids = 8, .tile_props = 1,
                             .animations = animated ? 2 : 0, .animation_frames = animated ? 3 : 0,
                             .tile_layers = 2, .object_layers = 1, .objects = 2, .properties = 5,
                             .string_bytes = 128};
    Bytes file(map_layout(caps).total);
    init_map_block(file.data(), caps, 16, 16);
    const MapView v = view(file);
    v.header().parallax_origin_x = 8;
    v.header().parallax_origin_y = 4;
    add_tileset(v, image);
    add_layers(v);
    add_objects(v);
    Header& h = v.header();
    PropertyEntry& music = add_property(v, {&h.properties_first, &h.properties_count}, "music", PropertyType::String);
    music.value = str(v, "theme");
    if (animated) add_animations(v);
    seal(file);
    return file;
}

} // namespace

Bytes sample(const char* image) { return sample_map(image, false); }

Bytes animated(const char* image) { return sample_map(image, true); }

Bytes minimal() {
    const MapCapacities caps{.width = 1, .height = 1, .string_bytes = 1};
    Bytes file(map_layout(caps).total);
    init_map_block(file.data(), caps, 16, 16);
    seal(file);
    return file;
}

Bytes two_tilesets(const std::uint16_t (&cells)[4], std::uint8_t mask) {
    const MapCapacities caps{.width = 4, .height = 1, .tilesets = 2, .tile_ids = 10, .tile_layers = 1,
                             .string_bytes = 8};
    Bytes file(map_layout(caps).total);
    init_map_block(file.data(), caps, 16, 16);
    const MapView v = view(file);
    v.tilesets()[0] = {.first_id = 1, .tile_count = 8, .columns = 4, .image_width = 64, .image_height = 32,
                       .name = str(v, "a"), .texture_slot = -1};
    v.tilesets()[1] = {.first_id = 9, .tile_count = 2, .columns = 2, .image_width = 32, .image_height = 16,
                       .name = str(v, "b"), .texture_slot = -1};
    for (int s = 0; s < 2; ++s) {
        const TilesetEntry& set = v.tilesets()[s];
        for (int i = 0; i < set.tile_count; ++i) v.tile_sources()[set.first_id + i] = tile_source(set, i, 16, 16);
    }
    v.header().tilesets = 2;
    v.header().tile_ids = 10;
    TileLayerEntry& layer = v.tile_layers()[0];
    layer.flags = kLayerVisible;
    layer.tilesets_used = mask;
    layer.parallax_x = layer.parallax_y = layer.opacity = 1;
    v.header().tile_layers = 1;
    for (int x = 0; x < 4; ++x) v.cells(0)[x] = cells[x];
    seal(file);
    return file;
}

Bytes from_map(const nnh::Tilemap& map) {
    const MapView source = map_view(map);
    Bytes file(source.header().file_size);
    std::memcpy(file.data(), source.base, file.size());
    const MapView v = view(file);
    v.header().collision_layers = 0;
    v.header().owned_textures = 0;
    for (int s = 0; s < v.header().tilesets; ++s) {
        v.tilesets()[s].texture_slot = -1;
        v.tilesets()[s].texture_generation = 0;
    }
    seal(file);
    return file;
}

void seal(Bytes& file) {
    Header& h = view(file).header();
    h.file_size = static_cast<std::uint32_t>(file.size());
    h.crc = crc32(file.data() + 16, file.size() - 16);
}

} // namespace map_file
