// nnh::Tilemap lifecycle: create (one block, tag "tilemap"), move, reset, basic queries.

#include <algorithm>
#include <cstring>

#include "gfx/gfx_state.hpp"
#include "log/engine_channels.hpp"
#include "nnh/log.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace nnh {

NNH_MEM_TAG(tilemap);

MemTag& detail::tilemap_memory_tag() { return NNH_MEM_TAG_REF(tilemap); }

Error detail::map_refuse(ErrorCode code, const char* what) {
    NNH_LOG_WARN(tilemap, "%s", what);
    return Error::make(code, "tilemap: %s", what);
}

detail::MapCapacities detail::capacities_for(const TilemapDesc& d) {
    return {.width = d.width,
            .height = d.height,
            .tilesets = d.max_tilesets,
            .tile_ids = d.max_tile_ids,
            .tile_props = std::max(0, std::min(d.max_properties, d.max_tile_ids)),
            .animations = d.max_animations,
            .animation_frames = d.max_animation_frames,
            .tile_layers = d.max_tile_layers,
            .object_layers = d.max_object_layers,
            .objects = d.max_objects,
            .properties = d.max_properties,
            .string_bytes = d.string_bytes < 0 ? d.string_bytes : std::max(1, d.string_bytes)};
}

Result<Tilemap> Tilemap::create(const TilemapDesc& desc) {
    using namespace detail::mapfmt;
    if (desc.tile_width < kMinTileSize || desc.tile_width > kMaxTileSize || desc.tile_height < kMinTileSize ||
        desc.tile_height > kMaxTileSize) {
        return detail::map_refuse(ErrorCode::InvalidArgument, "tile size must be 8..64 pixels");
    }
    const detail::MapCapacities caps = detail::capacities_for(desc);
    const detail::CapacityCheck check = detail::check_capacities(caps);
    if (!check.ok) {
        return detail::map_refuse(check.limit ? ErrorCode::LimitReached : ErrorCode::InvalidArgument, check.reason);
    }
    const std::uint32_t bytes = detail::map_layout(caps).total;
    void* memory = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(tilemap), bytes, 8);
    if (memory == nullptr) return detail::map_refuse(ErrorCode::OutOfMemory, "no memory for the map");
    std::memset(memory, 0, bytes);
    Tilemap map;
    map.block_ = static_cast<std::uint8_t*>(memory);
    detail::init_map_block(map.block_, caps, desc.tile_width, desc.tile_height);
    return map;
}

Tilemap::Tilemap(Tilemap&& other) noexcept : block_(other.block_) { other.block_ = nullptr; }

Tilemap& Tilemap::operator=(Tilemap&& other) noexcept {
    if (this != &other) {
        reset();
        block_ = other.block_;
        other.block_ = nullptr;
    }
    return *this;
}

void Tilemap::reset() {
    if (block_ != nullptr) {
        detail::gfx_forget_tiles(block_); // batches drawn this frame must not read it
        detail::release_map_textures(block_);
    }
    mem::free(block_);
    block_ = nullptr;
}

int Tilemap::width() const { return block_ ? detail::map_view(*this).header().width : 0; }
int Tilemap::height() const { return block_ ? detail::map_view(*this).header().height : 0; }
int Tilemap::tile_width() const { return block_ ? detail::map_view(*this).header().tile_width : 0; }
int Tilemap::tile_height() const { return block_ ? detail::map_view(*this).header().tile_height : 0; }
int Tilemap::tileset_count() const { return block_ ? detail::map_view(*this).header().tilesets : 0; }
int Tilemap::tile_id_count() const { return block_ ? detail::map_view(*this).header().tile_ids : 0; }

} // namespace nnh
