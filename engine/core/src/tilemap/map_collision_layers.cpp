// Collision layer sets of a map (spec 019 FR-6, D34): set and get the sets of tile layers and tile ids (kept in the
// runtime area after the format bytes), and the union of a cell's contributions.

#include "nnh/assert.hpp"
#include "tilemap/map_runtime.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace nnh {

namespace {

using namespace detail::mapfmt;
using collision::Layers;

} // namespace

void Tilemap::set_layer_collision_layers(int layer, Layers layers) {
    if (block_ == nullptr) return;
    const bool ok = layer >= 0 && layer < tile_layer_count() && layers != collision::kNoLayers;
    NNH_ASSERT(ok, "tilemap: collision layers 0x%X for tile layer %d (%d layers): a set can't be empty",
               static_cast<unsigned>(layers), layer, tile_layer_count());
    if (ok) detail::map_runtime(block_).layer_sets[layer] = layers;
}

Layers Tilemap::layer_collision_layers(int layer) const {
    if (block_ == nullptr || layer < 0 || layer >= tile_layer_count()) return collision::kNoLayers;
    return detail::map_runtime(block_).layer_sets[layer];
}

void Tilemap::set_tile_collision_layers(int tile_id, Layers layers) {
    if (block_ == nullptr) return;
    const bool ok = tile_id >= 1 && tile_id <= tile_id_count() && layers != collision::kNoLayers;
    NNH_ASSERT(ok, "tilemap: collision layers 0x%X for tile %d (%d ids): a set can't be empty",
               static_cast<unsigned>(layers), tile_id, tile_id_count());
    if (ok) detail::map_tile_sets(block_)[tile_id] = layers;
}

Layers Tilemap::tile_collision_layers(int tile_id) const {
    if (block_ == nullptr || tile_id < 1 || tile_id > tile_id_count()) return collision::kNoLayers;
    return detail::map_tile_sets(block_)[tile_id];
}

Layers Tilemap::collision_layers_at(int x, int y) const {
    if (block_ == nullptr || x < 0 || y < 0 || x >= width() || y >= height()) return collision::kNoLayers;
    const detail::MapView v = detail::map_view(*this);
    const Header& h = v.header();
    const detail::MapRuntime& r = detail::map_runtime(block_);
    const std::uint32_t* tile_sets = detail::map_tile_sets(block_);
    const std::uint8_t* flags = v.tile_flags();
    const int at = y * h.width + x;
    Layers all = collision::kNoLayers;
    for (unsigned mask = h.collision_layers, layer = 0; mask != 0; mask >>= 1, ++layer) {
        if ((mask & 1u) == 0) continue;
        const int id = cell_id(v.cells(static_cast<int>(layer))[at]);
        if (id == 0) continue;
        if ((v.tile_layers()[layer].flags & kLayerCollision) != 0) all |= r.layer_sets[layer];
        if ((flags[id] & kTileCollisionMask) != 0) all |= tile_sets[id];
    }
    return all;
}

} // namespace nnh
