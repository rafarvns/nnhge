// Building a map in code: add_tileset, add_tile_layer, add_object_layer, add_object.

#include <algorithm>

#include "gfx/gfx_state.hpp"
#include "nnh/texture.hpp"
#include "tilemap/map_strings.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace nnh {

namespace {

using namespace detail::mapfmt;

Error invalid(const char* what) { return detail::map_refuse(ErrorCode::InvalidArgument, what); }
Error full(const char* what) { return detail::map_refuse(ErrorCode::LimitReached, what); }

using detail::tiles_along;

void fill_sources(const detail::MapView& v, const TilesetEntry& set, int tile_w, int tile_h) {
    TileSource* sources = v.tile_sources();
    for (int i = 0; i < set.tile_count; ++i) sources[set.first_id + i] = detail::tile_source(set, i, tile_w, tile_h);
}

} // namespace

bool detail::tileset_texture_alive(const Tilemap& map, int tileset) {
    const MapView v = map_view(map);
    if (v.base == nullptr || tileset < 0 || tileset >= v.header().tilesets) return false;
    const TilesetEntry& set = v.tilesets()[tileset];
    const GfxState& g = gfx_state();
    return set.texture_slot >= 0 && g.ready && g.textures.get(set.texture_slot, set.texture_generation) != nullptr;
}

Result<int> Tilemap::add_tileset(const TilesetDesc& desc) {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr) return invalid("add_tileset on an empty map");
    if (desc.texture == nullptr || !desc.texture->valid()) return invalid("tileset needs a loaded texture");
    if (desc.margin < 0 || desc.spacing < 0 || desc.tile_count < 0) return invalid("negative tileset numbers");
    if (!detail::map_string_ok(desc.name)) return invalid("tileset name over 255 bytes");
    Header& h = v.header();
    const int columns = tiles_along(desc.texture->width(), h.tile_width, desc.margin, desc.spacing);
    const int rows = tiles_along(desc.texture->height(), h.tile_height, desc.margin, desc.spacing);
    if (columns * rows == 0) return invalid("no tile fits in the tileset texture");
    const int count = desc.tile_count == 0 ? columns * rows : desc.tile_count;
    if (count > columns * rows) return invalid("tile_count is more than fits in the texture");
    if (h.tilesets >= h.tilesets_cap) return full("max_tilesets reached");
    if (h.tile_ids + count > h.tile_ids_cap) return full("max_tile_ids reached");
    const int name = detail::map_intern(v, desc.name);
    if (name < 0) return full("string_bytes full");

    TilesetEntry& set = v.tilesets()[h.tilesets];
    set = {};
    set.first_id = static_cast<std::uint16_t>(h.tile_ids + 1);
    set.tile_count = static_cast<std::uint16_t>(count);
    set.columns = static_cast<std::uint16_t>(columns);
    set.margin = static_cast<std::uint16_t>(desc.margin);
    set.spacing = static_cast<std::uint16_t>(desc.spacing);
    set.image_width = static_cast<std::uint16_t>(desc.texture->width());
    set.image_height = static_cast<std::uint16_t>(desc.texture->height());
    set.name = static_cast<std::uint16_t>(name);
    set.texture_slot = TextureAccess::slot(*desc.texture);
    set.texture_generation = TextureAccess::generation(*desc.texture);
    fill_sources(v, set, h.tile_width, h.tile_height);
    h.tilesets += 1;
    h.tile_ids = static_cast<std::uint16_t>(h.tile_ids + count);
    return static_cast<int>(set.first_id);
}

Result<int> Tilemap::add_tile_layer(const TileLayerDesc& desc) {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr) return invalid("add_tile_layer on an empty map");
    if (!detail::map_string_ok(desc.name)) return invalid("layer name over 255 bytes");
    Header& h = v.header();
    if (h.tile_layers >= h.tile_layers_cap) return full("max_tile_layers reached");
    const int name = detail::map_intern(v, desc.name);
    if (name < 0) return full("string_bytes full");

    const int index = h.tile_layers;
    TileLayerEntry& layer = v.tile_layers()[index];
    layer.name = static_cast<std::uint16_t>(name);
    layer.flags = static_cast<std::uint8_t>((desc.visible ? kLayerVisible : 0) | (desc.collision ? kLayerCollision : 0));
    layer.tilesets_used = 0;
    layer.offset_x = desc.offset.x;
    layer.offset_y = desc.offset.y;
    layer.parallax_x = desc.parallax.x;
    layer.parallax_y = desc.parallax.y;
    layer.opacity = std::clamp(desc.opacity, 0.0f, 1.0f);
    if (desc.collision) h.collision_layers = static_cast<std::uint16_t>(h.collision_layers | (1u << index));
    h.tile_layers += 1;
    return index;
}

Result<int> Tilemap::add_object_layer(const char* name) {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr) return invalid("add_object_layer on an empty map");
    if (!detail::map_string_ok(name)) return invalid("object layer name over 255 bytes");
    Header& h = v.header();
    if (h.object_layers >= h.object_layers_cap) return full("max_object_layers reached");
    const int name_at = detail::map_intern(v, name);
    if (name_at < 0) return full("string_bytes full");
    const int index = h.object_layers;
    v.object_layers()[index] = {};
    v.object_layers()[index].name = static_cast<std::uint16_t>(name_at);
    h.object_layers += 1;
    return index;
}

Result<int> Tilemap::add_object(int object_layer, const ObjectDesc& desc) {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr) return invalid("add_object on an empty map");
    Header& h = v.header();
    if (object_layer < 0 || object_layer >= h.object_layers) return invalid("no such object layer");
    if (!(desc.bounds.w >= 0) || !(desc.bounds.h >= 0) || desc.id < 0) return invalid("negative object size or id");
    if (!detail::map_string_ok(desc.name) || !detail::map_string_ok(desc.type)) {
        return invalid("object name or type over 255 bytes");
    }
    if (h.objects >= h.objects_cap) return full("max_objects reached");
    const int name = detail::map_intern(v, desc.name);
    const int type = name < 0 ? -1 : detail::map_intern(v, desc.type);
    if (type < 0) return full("string_bytes full");

    std::uint32_t id = static_cast<std::uint32_t>(desc.id);
    if (id == 0) {
        for (int i = 0; i < h.objects; ++i) id = std::max(id, v.objects()[i].id);
        id += 1;
    }
    const bool point = desc.shape == nnh::ObjectShape::Point;
    ObjectEntry& o = v.objects()[h.objects];
    o = {};
    o.id = id;
    o.name = static_cast<std::uint16_t>(name);
    o.type = static_cast<std::uint16_t>(type);
    o.shape = static_cast<std::uint8_t>(point ? detail::mapfmt::ObjectShape::Point : detail::mapfmt::ObjectShape::Rect);
    o.object_layer = static_cast<std::uint16_t>(object_layer);
    o.x = desc.bounds.x;
    o.y = desc.bounds.y;
    o.w = point ? 0.0f : desc.bounds.w;
    o.h = point ? 0.0f : desc.bounds.h;
    ObjectLayerEntry& layer = v.object_layers()[object_layer];
    layer.object_count = static_cast<std::uint16_t>(layer.object_count + 1);
    return static_cast<int>(h.objects++);
}

} // namespace nnh
