// collision_grid(mask) (spec 019 FR-6, D34): a grid whose context is one of the map's mask slots, read live by
// grid_cell_masked. collision_grid() and its callback (map_collision.cpp) are not touched; this file holds the
// second copy of the cell rule, with one mask test per contribution (test_filtered_grid.cpp keeps them equal).

#include "log/engine_channels.hpp"
#include "nnh/assert.hpp"
#include "nnh/log.hpp"
#include "tilemap/map_runtime.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace nnh {

namespace {

using namespace detail::mapfmt;
using collision::Cell;
using collision::Layers;

/// Solid if a `collision` layer with a tile here is on `mask`, or a solid tile here is; else OneWay if a one-way
/// tile here is; else Empty. (x, y) is inside the map.
Cell masked_kind(std::uint8_t* block, Layers mask, int x, int y) {
    const detail::MapView v{block};
    const Header& h = v.header();
    const detail::MapRuntime& r = detail::map_runtime(block);
    const std::uint32_t* tile_sets = detail::map_tile_sets(block);
    const std::uint8_t* flags = v.tile_flags();
    const int at = y * h.width + x;
    Cell result = Cell::Empty;
    for (unsigned layers = h.collision_layers, layer = 0; layers != 0; layers >>= 1, ++layer) {
        if ((layers & 1u) == 0) continue;
        const int id = cell_id(v.cells(static_cast<int>(layer))[at]);
        if (id == 0) continue;
        if ((v.tile_layers()[layer].flags & kLayerCollision) != 0 && (r.layer_sets[layer] & mask) != 0) {
            return Cell::Solid;
        }
        const Cell kind = static_cast<Cell>(flags[id] & kTileCollisionMask);
        if (kind == Cell::Empty || (tile_sets[id] & mask) == 0) continue;
        if (kind == Cell::Solid) return Cell::Solid;
        result = Cell::OneWay;
    }
    return result;
}

Cell grid_cell_masked(const void* context, int x, int y) {
    const auto* slot = static_cast<const detail::GridMaskSlot*>(context);
    auto* block = const_cast<std::uint8_t*>(reinterpret_cast<const std::uint8_t*>(slot) - slot->block_offset);
    return masked_kind(block, slot->mask, x, y);
}

/// The slot holding `mask`, or a new one; null when all 8 are taken by other masks.
detail::GridMaskSlot* slot_for(std::uint8_t* block, Layers mask) {
    detail::MapRuntime& r = detail::map_runtime(block);
    for (detail::GridMaskSlot& slot : r.grid_masks) {
        if (slot.mask == mask) return &slot;
        if (slot.mask != collision::kAllLayers) continue;
        slot.mask = mask; // first use: registered for good (grids handed out keep pointing here)
        slot.block_offset = static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t*>(&slot) - block);
        return &slot;
    }
    return nullptr;
}

} // namespace

collision::Grid Tilemap::collision_grid(Layers mask) const {
    if (block_ == nullptr || mask == collision::kAllLayers) return collision_grid();
    detail::GridMaskSlot* slot = slot_for(block_, mask);
    NNH_ASSERT(slot != nullptr, "tilemap: more than %d collision_grid() masks on one map", detail::kMaxGridMasks);
    if (slot == nullptr) {
        detail::MapRuntime& r = detail::map_runtime(block_);
        if ((r.flags & detail::kRuntimeOutOfMasksLogged) == 0) {
            NNH_LOG_ERROR(tilemap, "collision_grid(0x%X): a map holds %d masks; using the unfiltered grid",
                          static_cast<unsigned>(mask), detail::kMaxGridMasks);
            r.flags |= detail::kRuntimeOutOfMasksLogged;
        }
        return collision_grid(); // walls stay walls
    }
    collision::Grid grid = collision_grid();
    grid.cell = grid_cell_masked;
    grid.context = slot;
    return grid;
}

} // namespace nnh
