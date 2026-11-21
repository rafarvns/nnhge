// Map capacities → layout, limit checks, fresh blocks (see map_layout.hpp).

#include "tilemap/map_layout.hpp"

#include <cstring>

namespace nnh::detail {

namespace {

using namespace mapfmt;

std::uint32_t align4(std::uint32_t n) { return (n + 3u) & ~3u; }
std::uint32_t u(int v) { return static_cast<std::uint32_t>(v); }

CapacityCheck invalid(const char* reason) { return {false, false, reason}; }
CapacityCheck over(const char* reason) { return {false, true, reason}; }

} // namespace

MapLayout map_layout(const MapCapacities& c) {
    const std::uint32_t ids = u(c.tile_ids) + 1;
    MapLayout l;
    l.sizes[kTilesets] = u(c.tilesets) * sizeof(TilesetEntry);
    l.sizes[kTileSources] = ids * sizeof(TileSource);
    l.sizes[kTileFlags] = align4(ids);
    l.sizes[kTileProps] = align4(u(c.tile_props) * sizeof(TilePropsEntry));
    l.sizes[kAnimations] = u(c.animations) * sizeof(AnimationEntry);
    l.sizes[kFrames] = u(c.animation_frames) * sizeof(FrameEntry);
    l.sizes[kTileLayers] = u(c.tile_layers) * sizeof(TileLayerEntry);
    l.sizes[kCells] = align4(u(c.width) * u(c.height) * u(c.tile_layers) * 2u);
    l.sizes[kObjectLayers] = u(c.object_layers) * sizeof(ObjectLayerEntry);
    l.sizes[kObjects] = u(c.objects) * sizeof(ObjectEntry);
    l.sizes[kProperties] = u(c.properties) * sizeof(PropertyEntry);
    l.sizes[kStrings] = align4(u(c.string_bytes));
    std::uint32_t at = sizeof(Header);
    for (int s = 0; s < kSectionCount; ++s) {
        l.offsets[s] = at;
        at += l.sizes[s];
    }
    l.total = at;
    return l;
}

CapacityCheck check_capacities(const MapCapacities& c) {
    if (c.width < 1 || c.height < 1) return invalid("map size must be at least 1x1 cells");
    if (c.tilesets < 0 || c.tile_ids < 0 || c.tile_props < 0 || c.animations < 0 || c.animation_frames < 0 ||
        c.tile_layers < 0 || c.object_layers < 0 || c.objects < 0 || c.properties < 0 || c.string_bytes < 0) {
        return invalid("capacities can't be negative");
    }
    if (c.width > kMaxMapSize || c.height > kMaxMapSize) return over("map size over 1024x1024 cells");
    if (c.tile_layers > kMaxTileLayers) return over("more than 16 tile layers");
    if (c.tilesets > kMaxTilesets) return over("more than 8 tilesets");
    if (c.tile_ids > kMaxTileIds || c.tile_props > kMaxTileIds) return over("more than 8191 tile ids");
    if (c.object_layers > kMaxObjectLayers) return over("more than 16 object layers");
    if (c.objects > kMaxObjects) return over("more than 4096 objects");
    if (c.properties > kMaxProperties) return over("more than 16384 properties");
    if (c.string_bytes > kMaxStringBytes) return over("more than 64 KiB of strings");
    if (c.animations > kMaxAnimations) return over("more than 256 animated tiles");
    if (c.animation_frames > kMaxAnimationFrames) return over("more than 4096 animation frames");
    if (c.width * c.height * c.tile_layers > kMaxTotalCells) return over("more than 4 Mi cells in all layers");
    return {};
}

MapCapacities capacities_of(const Header& h) {
    return {.width = h.width,
            .height = h.height,
            .tilesets = h.tilesets_cap,
            .tile_ids = h.tile_ids_cap,
            .tile_props = h.tile_props_cap,
            .animations = h.animations_cap,
            .animation_frames = h.frames_cap,
            .tile_layers = h.tile_layers_cap,
            .object_layers = h.object_layers_cap,
            .objects = h.objects_cap,
            .properties = h.properties_cap,
            .string_bytes = static_cast<int>(h.string_cap)};
}

void init_map_block(std::uint8_t* block, const MapCapacities& c, int tile_width, int tile_height) {
    const MapLayout l = map_layout(c);
    Header& h = *reinterpret_cast<Header*>(block);
    std::memcpy(h.magic, kMagic, sizeof h.magic);
    h.version = kVersion;
    h.file_size = l.total;
    h.width = static_cast<std::uint16_t>(c.width);
    h.height = static_cast<std::uint16_t>(c.height);
    h.tile_width = static_cast<std::uint16_t>(tile_width);
    h.tile_height = static_cast<std::uint16_t>(tile_height);
    h.tilesets_cap = static_cast<std::uint8_t>(c.tilesets);
    h.tile_layers_cap = static_cast<std::uint8_t>(c.tile_layers);
    h.object_layers_cap = static_cast<std::uint8_t>(c.object_layers);
    h.tile_ids_cap = static_cast<std::uint16_t>(c.tile_ids);
    h.tile_props_cap = static_cast<std::uint16_t>(c.tile_props);
    h.animations_cap = static_cast<std::uint16_t>(c.animations);
    h.frames_cap = static_cast<std::uint16_t>(c.animation_frames);
    h.objects_cap = static_cast<std::uint16_t>(c.objects);
    h.properties_cap = static_cast<std::uint16_t>(c.properties);
    h.string_cap = u(c.string_bytes);
    h.string_bytes = c.string_bytes > 0 ? 1 : 0; // the empty string (already zero)
    for (int s = 0; s < kSectionCount; ++s) h.offsets[s] = l.offsets[s];
    const std::uint32_t layer_bytes = u(c.width) * u(c.height) * 2u;
    auto* layers = reinterpret_cast<TileLayerEntry*>(block + l.offsets[kTileLayers]);
    for (int i = 0; i < c.tile_layers; ++i) layers[i].cells = l.offsets[kCells] + u(i) * layer_bytes;
}

} // namespace nnh::detail
