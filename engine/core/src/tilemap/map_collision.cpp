// Collision of a map: tile collision flags, `collision` layers, the contributing-layer mask, collision_at
// and collision_grid (spec 012 FR-7, FR-8). The map never calls collision::move; it only describes cells.

#include "tilemap/tilemap_internal.hpp"

namespace nnh {

namespace {

using namespace detail::mapfmt;
using collision::Cell;

/// True when some cell of `layer` holds a tile marked solid or one-way.
bool layer_has_collision_tile(const detail::MapView& v, int layer) {
    const Header& h = v.header();
    const std::uint16_t* cells = v.cells(layer);
    const std::uint8_t* flags = v.tile_flags();
    const int count = h.width * h.height;
    for (int i = 0; i < count; ++i) {
        if ((flags[cell_id(cells[i])] & kTileCollisionMask) != 0) return true;
    }
    return false;
}

void set_mask_bit(Header& h, int layer, bool on) {
    const unsigned bit = 1u << layer;
    h.collision_layers = static_cast<std::uint16_t>(on ? (h.collision_layers | bit) : (h.collision_layers & ~bit));
}

} // namespace

void Tilemap::set_layer_collision(int layer, bool collision) {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr || layer < 0 || layer >= v.header().tile_layers) return;
    TileLayerEntry& e = v.tile_layers()[layer];
    e.flags = static_cast<std::uint8_t>(collision ? (e.flags | kLayerCollision) : (e.flags & ~kLayerCollision));
    set_mask_bit(v.header(), layer, collision || layer_has_collision_tile(v, layer));
}

namespace {

bool layer_has_tile(const detail::MapView& v, int layer, int id) {
    const Header& h = v.header();
    const std::uint16_t* cells = v.cells(layer);
    const int count = h.width * h.height;
    for (int i = 0; i < count; ++i) {
        if (cell_id(cells[i]) == id) return true;
    }
    return false;
}

int tileset_of(const detail::MapView& v, int id) {
    for (int s = 0; s < v.header().tilesets; ++s) {
        const TilesetEntry& set = v.tilesets()[s];
        if (id >= set.first_id && id < set.first_id + set.tile_count) return s;
    }
    return -1;
}

/// Solid > OneWay > Empty over the layers that may collide. (x, y) must be inside the map.
Cell cell_kind(const detail::MapView& v, int x, int y) {
    const Header& h = v.header();
    const std::uint8_t* flags = v.tile_flags();
    const int at = y * h.width + x;
    Cell result = Cell::Empty;
    for (unsigned mask = h.collision_layers, layer = 0; mask != 0; mask >>= 1, ++layer) {
        if ((mask & 1u) == 0) continue;
        const int id = cell_id(v.cells(static_cast<int>(layer))[at]);
        if (id == 0) continue;
        if ((v.tile_layers()[layer].flags & kLayerCollision) != 0) return Cell::Solid;
        const Cell kind = static_cast<Cell>(flags[id] & kTileCollisionMask);
        if (kind == Cell::Solid) return Cell::Solid;
        if (kind == Cell::OneWay) result = Cell::OneWay;
    }
    return result;
}

Cell grid_cell(const void* context, int x, int y) {
    return cell_kind(detail::MapView{static_cast<std::uint8_t*>(const_cast<void*>(context))}, x, y);
}

} // namespace

void Tilemap::set_tile_collision(int tile_id, Cell kind) {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr) return;
    Header& h = v.header();
    const bool known = tile_id >= 1 && tile_id <= h.tile_ids;
    NNH_ASSERT(known, "tilemap: no tile id %d (%d ids)", tile_id, h.tile_ids);
    if (!known) return;
    std::uint8_t& flags = v.tile_flags()[tile_id];
    flags = static_cast<std::uint8_t>((flags & ~kTileCollisionMask) | static_cast<std::uint8_t>(kind));
    if (kind == Cell::Empty) return; // the mask may stay wider than needed: costs time, never correctness
    const unsigned set_bit = 1u << tileset_of(v, tile_id);
    for (int layer = 0; layer < h.tile_layers; ++layer) {
        const bool in_mask = (h.collision_layers >> layer) & 1u;
        if (!in_mask && (v.tile_layers()[layer].tilesets_used & set_bit) != 0 && layer_has_tile(v, layer, tile_id)) {
            set_mask_bit(h, layer, true);
        }
    }
}

Cell Tilemap::tile_collision(int tile_id) const {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr || tile_id < 1 || tile_id > v.header().tile_ids) return Cell::Empty;
    return static_cast<Cell>(v.tile_flags()[tile_id] & kTileCollisionMask);
}

Cell Tilemap::collision_at(int x, int y) const {
    if (block_ == nullptr || x < 0 || y < 0 || x >= width() || y >= height()) return Cell::Empty;
    return cell_kind(detail::map_view(*this), x, y);
}

collision::Grid Tilemap::collision_grid() const {
    if (block_ == nullptr) return {};
    collision::Grid grid;
    grid.cell_w = static_cast<float>(tile_width());
    grid.cell_h = static_cast<float>(tile_height());
    grid.width = width();
    grid.height = height();
    grid.cell = grid_cell;
    grid.context = block_;
    return grid;
}

} // namespace nnh
