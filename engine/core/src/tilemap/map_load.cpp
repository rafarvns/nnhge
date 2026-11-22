// load_tilemap (spec 012 stage 4): asset → validate → copy with the LoadTilemapOptions extras → tileset textures,
// which the map owns (released by Tilemap::reset through release_map_textures).

#include <cstdio>
#include <cstring>
#include <utility>

#include "assets/asset_path.hpp"
#include "gfx/gfx_state.hpp"
#include "log/engine_channels.hpp"
#include "nnh/assets.hpp"
#include "nnh/log.hpp"
#include "nnh/texture.hpp"
#include "tilemap/map_copy.hpp"
#include "tilemap/map_validate.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace nnh {

namespace {

using namespace detail::mapfmt;

ErrorCode code_of(detail::MapProblem problem) {
    switch (problem) {
    case detail::MapProblem::NewerVersion: return ErrorCode::Unsupported;
    case detail::MapProblem::OverLimit: return ErrorCode::LimitReached;
    default: return ErrorCode::Corrupted;
    }
}

Error refuse(ErrorCode code, const char* path, const char* what) {
    NNH_LOG_WARN(tilemap, "load_tilemap(\"%s\"): %s", path, what);
    return Error::make(code, "map: %s", what);
}

/// Points tileset `s` at its image's texture: an earlier tileset's with the same image, or a new one the map owns.
Result<void> load_tileset_texture(const detail::MapView& v, int s, const char* path) {
    TilesetEntry& set = v.tilesets()[s];
    const char* image = v.string(set.name);
    for (int i = 0; i < s; ++i) {
        const TilesetEntry& earlier = v.tilesets()[i];
        if (std::strcmp(v.string(earlier.name), image) == 0) {
            set.texture_slot = earlier.texture_slot;
            set.texture_generation = earlier.texture_generation;
            return {};
        }
    }
    auto texture = load_texture(image);
    if (!texture) {
        NNH_LOG_WARN(tilemap, "load_tilemap(\"%s\"): tileset image \"%s\": %s", path, image, texture.error().message);
        return Error::make(texture.error().code, "tileset image \"%s\": %s", image, texture.error().message);
    }
    if (texture.value().width() != set.image_width || texture.value().height() != set.image_height) {
        NNH_LOG_WARN(tilemap, "load_tilemap(\"%s\"): tileset image \"%s\" is %dx%d, the map was made for %dx%d", path,
                     image, texture.value().width(), texture.value().height(), set.image_width, set.image_height);
        return Error::make(ErrorCode::Corrupted, "tileset image \"%s\" changed size", image);
    }
    set.texture_slot = TextureAccess::slot(texture.value());
    set.texture_generation = TextureAccess::generation(texture.value());
    TextureAccess::release(texture.value()); // the map frees it
    Header& h = v.header();
    h.owned_textures = static_cast<std::uint8_t>(h.owned_textures | (1u << s));
    return {};
}

/// Reads the asset and makes the map; the file's buffer is freed on return, before any image is loaded.
Result<Tilemap> read_map(const char* asset, const char* path, const LoadTilemapOptions& options) {
    auto file = assets::load(asset);
    if (!file) return file.error();
    return detail::tilemap_from_file(file.value().data(), file.value().size(), options, path);
}

} // namespace

Result<Tilemap> detail::tilemap_from_file(const void* data, std::size_t size, const LoadTilemapOptions& options,
                                          const char* name) {
    const MapCheck check = validate_map(data, size);
    if (!check.ok()) return refuse(code_of(check.problem), name, check.reason);
    Header file;
    std::memcpy(&file, data, sizeof file);
    MapCapacities caps;
    const MapExtras extras{options.extra_tile_layers, options.extra_objects, options.extra_properties,
                           options.extra_string_bytes};
    const CapacityCheck room = loaded_capacities(file, extras, caps);
    if (!room.ok) return refuse(room.limit ? ErrorCode::LimitReached : ErrorCode::InvalidArgument, name, room.reason);
    const std::uint32_t bytes = map_layout(caps).total;
    void* memory = mem::alloc(MemRegion::Heap, tilemap_memory_tag(), bytes, 8);
    if (memory == nullptr) return refuse(ErrorCode::OutOfMemory, name, "no memory for the map");
    std::memset(memory, 0, bytes);
    copy_map(static_cast<const std::uint8_t*>(data), caps, static_cast<std::uint8_t*>(memory));
    return TilemapAccess::adopt(static_cast<std::uint8_t*>(memory));
}

void detail::release_map_textures(std::uint8_t* block) {
    const MapView v{block};
    Header& h = v.header();
    for (int s = 0; s < h.tilesets; ++s) {
        if (((h.owned_textures >> s) & 1u) == 0) continue;
        const TilesetEntry& set = v.tilesets()[s];
        Texture owned = TextureAccess::make(set.texture_slot, set.texture_generation, set.image_width, set.image_height);
        owned.reset();
    }
    h.owned_textures = 0;
}

Result<Tilemap> load_tilemap(const char* path, const LoadTilemapOptions& options) {
    if (path == nullptr) return Error::make(ErrorCode::InvalidArgument, "load_tilemap(nullptr)");
    if (!detail::gfx_state().ready) return Error::make(ErrorCode::Unsupported, "load_tilemap before nnh::init()");
    char asset[detail::kMaxAssetPath + 1];
    if (std::snprintf(asset, sizeof asset, "%s.nnhmap", path) >= static_cast<int>(sizeof asset)) {
        return refuse(ErrorCode::InvalidArgument, path, "path too long");
    }
    auto made = read_map(asset, path, options);
    if (!made) return made.error();
    Tilemap map = std::move(made.value());
    const detail::MapView v = detail::map_view(map);
    for (int s = 0; s < v.header().tilesets; ++s) {
        auto loaded = load_tileset_texture(v, s, path);
        if (!loaded) return loaded.error(); // the map frees the images it loaded so far
    }
    return map;
}

} // namespace nnh
