// A validated `.nnhmap` file → a map block with extra capacity (see map_copy.hpp).

#include "tilemap/map_copy.hpp"

#include <algorithm>
#include <cstring>

namespace nnh::detail {

namespace {

using namespace mapfmt;

/// Layers that can collide: `collision` layers and layers holding a solid or one-way tile.
std::uint16_t collision_mask(const MapView& v) {
    const Header& h = v.header();
    const std::uint8_t* flags = v.tile_flags();
    const int count = h.width * h.height;
    unsigned mask = 0;
    for (int layer = 0; layer < h.tile_layers; ++layer) {
        bool collides = (v.tile_layers()[layer].flags & kLayerCollision) != 0;
        const std::uint16_t* cells = v.cells(layer);
        for (int i = 0; !collides && i < count; ++i) collides = (flags[cell_id(cells[i])] & kTileCollisionMask) != 0;
        if (collides) mask |= 1u << layer;
    }
    return static_cast<std::uint16_t>(mask);
}

void copy_counts(const Header& f, Header& h) {
    h.parallax_origin_x = f.parallax_origin_x;
    h.parallax_origin_y = f.parallax_origin_y;
    h.properties_first = f.properties_first;
    h.properties_count = f.properties_count;
    h.tilesets = f.tilesets;
    h.tile_layers = f.tile_layers;
    h.object_layers = f.object_layers;
    h.tile_ids = f.tile_ids;
    h.tile_props = f.tile_props;
    h.animations = f.animations;
    h.frames = f.frames;
    h.objects = f.objects;
    h.properties = f.properties;
    h.string_bytes = f.string_bytes;
}

} // namespace

CapacityCheck loaded_capacities(const Header& file, const MapExtras& e, MapCapacities& out) {
    if (e.tile_layers < 0 || e.objects < 0 || e.properties < 0 || e.string_bytes < 0) {
        return {false, false, "load options can't be negative"};
    }
    // Each extra alone over its limit: refused before adding, so the sums below can't overflow.
    if (e.tile_layers > kMaxTileLayers || e.objects > kMaxObjects || e.properties > kMaxProperties ||
        e.string_bytes > kMaxStringBytes) {
        return {false, true, "load options over the map limits"};
    }
    out = capacities_of(file);
    out.tile_layers += e.tile_layers;
    out.objects += e.objects;
    out.properties += e.properties;
    out.string_bytes += e.string_bytes;
    out.tile_props = std::max(out.tile_props, std::min(out.tile_props + e.properties, out.tile_ids));
    return check_capacities(out);
}

void copy_map(const std::uint8_t* file, const MapCapacities& caps, std::uint8_t* block) {
    Header f;
    std::memcpy(&f, file, sizeof f);
    init_map_block(block, caps, f.tile_width, f.tile_height);
    const MapView v{block};
    Header& h = v.header();
    copy_counts(f, h);
    const auto copy = [&](Section s, std::size_t bytes) { std::memcpy(block + h.offsets[s], file + f.offsets[s], bytes); };
    copy(kTilesets, f.tilesets * sizeof(TilesetEntry));
    copy(kTileSources, (f.tile_ids + 1u) * sizeof(TileSource));
    copy(kTileFlags, f.tile_ids + 1u);
    copy(kTileProps, f.tile_props * sizeof(TilePropsEntry));
    copy(kAnimations, f.animations * sizeof(AnimationEntry));
    copy(kFrames, f.frames * sizeof(FrameEntry));
    copy(kTileLayers, f.tile_layers * sizeof(TileLayerEntry));
    copy(kObjectLayers, f.object_layers * sizeof(ObjectLayerEntry));
    copy(kObjects, f.objects * sizeof(ObjectEntry));
    copy(kProperties, f.properties * sizeof(PropertyEntry));
    copy(kStrings, f.string_bytes);
    const std::uint32_t layer_bytes = 2u * f.width * f.height;
    for (int layer = 0; layer < f.tile_layers; ++layer) {
        TileLayerEntry& e = v.tile_layers()[layer];
        const std::uint32_t cells = h.offsets[kCells] + static_cast<std::uint32_t>(layer) * layer_bytes;
        std::memcpy(block + cells, file + e.cells, layer_bytes); // e.cells is still the file's offset here
        e.cells = cells;
    }
    for (int s = 0; s < f.tilesets; ++s) {
        v.tilesets()[s].texture_slot = -1;
        v.tilesets()[s].texture_generation = 0;
    }
    h.owned_textures = 0;
    h.collision_layers = collision_mask(v);
}

} // namespace nnh::detail
