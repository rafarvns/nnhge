// Tiles, coordinates and tile layer settings of a map.

#include <algorithm>
#include <cmath>
#include <cstring>

#include "tilemap/tilemap_internal.hpp"

namespace nnh {

namespace {

using namespace detail::mapfmt;

/// The layer entry, or null for an empty map or an invalid index.
TileLayerEntry* layer_entry(const Tilemap& map, int layer) {
    const detail::MapView v = detail::map_view(map);
    if (v.base == nullptr || layer < 0 || layer >= v.header().tile_layers) return nullptr;
    return &v.tile_layers()[layer];
}

} // namespace

int Tilemap::tile_layer_count() const { return block_ ? detail::map_view(*this).header().tile_layers : 0; }

int Tilemap::find_tile_layer(const char* name) const {
    if (name == nullptr) return -1;
    for (int i = 0; i < tile_layer_count(); ++i) {
        if (std::strcmp(tile_layer_name(i), name) == 0) return i;
    }
    return -1;
}

const char* Tilemap::tile_layer_name(int layer) const {
    const TileLayerEntry* e = layer_entry(*this, layer);
    return e ? detail::map_view(*this).string(e->name) : "";
}

void Tilemap::set_layer_offset(int layer, Vec2 offset) {
    if (TileLayerEntry* e = layer_entry(*this, layer)) {
        e->offset_x = offset.x;
        e->offset_y = offset.y;
    }
}

void Tilemap::set_layer_parallax(int layer, Vec2 factor) {
    if (TileLayerEntry* e = layer_entry(*this, layer)) {
        e->parallax_x = factor.x;
        e->parallax_y = factor.y;
    }
}

void Tilemap::set_layer_opacity(int layer, float opacity) {
    if (TileLayerEntry* e = layer_entry(*this, layer)) e->opacity = std::clamp(opacity, 0.0f, 1.0f);
}

void Tilemap::set_layer_visible(int layer, bool visible) {
    if (TileLayerEntry* e = layer_entry(*this, layer)) {
        e->flags = static_cast<std::uint8_t>(visible ? (e->flags | kLayerVisible) : (e->flags & ~kLayerVisible));
    }
}

Vec2 Tilemap::layer_offset(int layer) const {
    const TileLayerEntry* e = layer_entry(*this, layer);
    return e ? Vec2{e->offset_x, e->offset_y} : Vec2{};
}

Vec2 Tilemap::layer_parallax(int layer) const {
    const TileLayerEntry* e = layer_entry(*this, layer);
    return e ? Vec2{e->parallax_x, e->parallax_y} : Vec2{};
}

float Tilemap::layer_opacity(int layer) const {
    const TileLayerEntry* e = layer_entry(*this, layer);
    return e ? e->opacity : 0.0f;
}

bool Tilemap::layer_visible(int layer) const {
    const TileLayerEntry* e = layer_entry(*this, layer);
    return e && (e->flags & kLayerVisible) != 0;
}

bool Tilemap::layer_collision(int layer) const {
    const TileLayerEntry* e = layer_entry(*this, layer);
    return e && (e->flags & kLayerCollision) != 0;
}

void Tilemap::set_parallax_origin(Vec2 origin) {
    if (block_ == nullptr) return;
    Header& h = detail::map_view(*this).header();
    h.parallax_origin_x = origin.x;
    h.parallax_origin_y = origin.y;
}

Vec2 Tilemap::parallax_origin() const {
    if (block_ == nullptr) return {};
    const Header& h = detail::map_view(*this).header();
    return {h.parallax_origin_x, h.parallax_origin_y};
}

namespace {

constexpr float kCellClamp = 1073741824.0f; // 2^30, like collision's cell ranges

int floor_cell(float world, int size) {
    const float cell = std::floor(world / static_cast<float>(size));
    return static_cast<int>(std::clamp(cell, -kCellClamp, kCellClamp));
}

/// Checks a tile before it is written to `layer` (asserts on misuse). False: do not write it.
bool tile_ok(const detail::MapView& v, int layer, Tile t) {
    const Header& h = v.header();
    const bool layer_ok = layer >= 0 && layer < h.tile_layers;
    NNH_ASSERT(layer_ok, "tilemap: no tile layer %d (%d layers)", layer, h.tile_layers);
    NNH_ASSERT(t.id <= h.tile_ids, "tilemap: no tile id %d (%d ids)", t.id, h.tile_ids);
    const bool flips_ok = !t.flip_diagonal || h.tile_width == h.tile_height;
    NNH_ASSERT(flips_ok, "tilemap: diagonal flips need square tiles");
    return layer_ok && t.id <= h.tile_ids && flips_ok;
}

/// Writes a checked tile to cells [x0, x1) x [y0, y1) (already clipped) and updates the layer's masks.
void write_cells(const detail::MapView& v, int layer, int x0, int y0, int x1, int y1, Tile t) {
    Header& h = v.header();
    const std::uint16_t cell = t.id == 0 ? 0 : encode_cell(t.id, t.flip_x, t.flip_y, t.flip_diagonal);
    std::uint16_t* cells = v.cells(layer);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) cells[y * h.width + x] = cell;
    }
    if (t.id == 0 || x0 >= x1 || y0 >= y1) return;
    TileLayerEntry& e = v.tile_layers()[layer];
    for (int s = 0; s < h.tilesets; ++s) {
        const TilesetEntry& set = v.tilesets()[s];
        if (t.id >= set.first_id && t.id < set.first_id + set.tile_count) {
            e.tilesets_used = static_cast<std::uint8_t>(e.tilesets_used | (1u << s));
        }
    }
    if ((v.tile_flags()[t.id] & kTileCollisionMask) != 0) {
        h.collision_layers = static_cast<std::uint16_t>(h.collision_layers | (1u << layer));
    }
}

} // namespace

Rect Tilemap::cell_rect(int x, int y) const {
    const float w = static_cast<float>(tile_width());
    const float h = static_cast<float>(tile_height());
    return {static_cast<float>(x) * w, static_cast<float>(y) * h, w, h};
}

bool Tilemap::world_to_cell(Vec2 world, int& x, int& y) const {
    if (block_ == nullptr || std::isnan(world.x) || std::isnan(world.y)) {
        x = y = 0;
        return false;
    }
    x = floor_cell(world.x, tile_width());
    y = floor_cell(world.y, tile_height());
    return x >= 0 && y >= 0 && x < width() && y < height();
}

Tile Tilemap::tile_at(int layer, Vec2 world) const {
    int x = 0, y = 0;
    return world_to_cell(world, x, y) ? tile(layer, x, y) : Tile{};
}

void Tilemap::set_tile(int layer, int x, int y, Tile t) { fill(layer, x, y, 1, 1, t); }

void Tilemap::fill(int layer, int x, int y, int w, int h, Tile t) {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr || !tile_ok(v, layer, t)) return;
    // Clip in 64-bit-free arithmetic: x + w may overflow int for huge w, so clamp each edge separately.
    const int x0 = std::max(x, 0);
    const int y0 = std::max(y, 0);
    const int x1 = w <= 0 ? x0 : (x >= width() - w ? width() : std::max(x + w, 0));
    const int y1 = h <= 0 ? y0 : (y >= height() - h ? height() : std::max(y + h, 0));
    write_cells(v, layer, x0, y0, std::max(x0, x1), std::max(y0, y1), t);
}

Tile Tilemap::tile(int layer, int x, int y) const {
    if (layer_entry(*this, layer) == nullptr || x < 0 || y < 0 || x >= width() || y >= height()) return {};
    const std::uint16_t cell = detail::map_view(*this).cells(layer)[y * width() + x];
    return {static_cast<std::uint16_t>(cell_id(cell)), cell_flip_x(cell), cell_flip_y(cell), cell_diagonal(cell)};
}

} // namespace nnh
