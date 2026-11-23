// The runtime area of a map block (spec 019, D34), see map_runtime.hpp.

#include "tilemap/map_runtime.hpp"

#include "nnh/collision.hpp"

namespace nnh::detail {

std::uint32_t map_block_bytes(const MapCapacities& capacities) {
    const auto ids = static_cast<std::uint32_t>(capacities.tile_ids) + 1;
    return map_layout(capacities).total + static_cast<std::uint32_t>(sizeof(MapRuntime)) + 4u * ids;
}

void init_map_runtime(std::uint8_t* block) {
    MapRuntime& r = map_runtime(block);
    for (std::uint32_t& set : r.layer_sets) set = collision::kDefaultLayer;
    for (GridMaskSlot& slot : r.grid_masks) slot = {collision::kAllLayers, 0};
    r.flags = 0;
    std::uint32_t* tiles = map_tile_sets(block);
    const int ids = MapView{block}.header().tile_ids_cap + 1;
    for (int i = 0; i < ids; ++i) tiles[i] = collision::kDefaultLayer;
}

} // namespace nnh::detail
