#pragma once
// The runtime area of a map block (spec 019, D34): collision layer sets and the slots of filtered grids, right
// after the `.nnhmap` bytes (header.file_size, 4-aligned), never in a file. Engine only: nnh-assetc, the format
// and its validator know nothing of it.
//
//   block:  [ .nnhmap bytes (header.file_size) ][ MapRuntime (132 B) ][ u32 tile_sets[tile_ids_cap + 1] ]

#include <cstdint>

#include "tilemap/map_layout.hpp"

namespace nnh::detail {

/// Distinct masks a map can hand out filtered grids for (collision_grid(mask)); a slot is never reused, so a grid
/// handed out earlier stays valid.
inline constexpr int kMaxGridMasks = 8;

/// One filtered grid's context: its mask, and where the block starts (the callback finds the map from it).
struct GridMaskSlot {
    std::uint32_t mask;         ///< collision::kAllLayers in an unused slot
    std::uint32_t block_offset; ///< bytes from the block's start to this slot
};

struct MapRuntime {
    std::uint32_t layer_sets[mapfmt::kMaxTileLayers]; ///< collision layer set of each tile layer
    GridMaskSlot grid_masks[kMaxGridMasks];
    std::uint32_t flags; ///< kRuntimeOutOfMasksLogged
    // followed by std::uint32_t tile_sets[tile_ids_cap + 1]: collision layer set of each tile id
};
static_assert(sizeof(MapRuntime) == 132);

inline constexpr std::uint32_t kRuntimeOutOfMasksLogged = 1u;

/// The block size of a map with `capacities`: the format's layout, then the runtime area.
std::uint32_t map_block_bytes(const MapCapacities& capacities);

/// Every layer set and tile set at the default layer, every slot unused. After init_map_block / copy_map.
void init_map_runtime(std::uint8_t* block);

inline MapRuntime& map_runtime(std::uint8_t* block) {
    return *reinterpret_cast<MapRuntime*>(block + MapView{block}.header().file_size);
}
inline std::uint32_t* map_tile_sets(std::uint8_t* block) {
    return reinterpret_cast<std::uint32_t*>(&map_runtime(block) + 1);
}

} // namespace nnh::detail
