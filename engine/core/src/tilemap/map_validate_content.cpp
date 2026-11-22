// Bounded validation of a `.nnhmap` file, part 2 (see map_validate.hpp): tile layers and their cells, object
// layers and objects, properties and every owner's property range. Runs after map_validate.cpp's checks.

#include "tilemap/map_validate_parts.hpp"

namespace nnh::detail::mapcheck {

namespace {

using namespace mapfmt;

constexpr std::uint8_t kKnownLayerFlags = kLayerVisible | kLayerCollision;

/// The tilesets the cells of one layer use, or a problem with a cell.
MapCheck scan_cells(const Reader& r, std::uint32_t cells, unsigned& used) {
    const Header& h = r.h;
    const bool square = h.tile_width == h.tile_height;
    const int count = h.width * h.height;
    used = 0;
    for (int i = 0; i < count; ++i) {
        const std::uint16_t cell = r.u16(cells + 2u * static_cast<std::uint32_t>(i));
        const int id = cell_id(cell);
        if (id > h.tile_ids) return damaged("cell with an unknown tile id");
        if (cell_diagonal(cell) && !square) return damaged("diagonal flip on a map with non-square tiles");
        if (id != 0) used |= 1u << r.tileset_of(id);
    }
    return {};
}

MapCheck check_layer(const Reader& r, int index) {
    const Header& h = r.h;
    const TileLayerEntry e = r.at<TileLayerEntry>(kTileLayers, index);
    if (!r.string_ok(e.name)) return damaged("tile layer name: string offset outside the pool");
    if ((e.flags & ~kKnownLayerFlags) != 0) return damaged("unknown tile layer flags");
    if (!finite(e.offset_x) || !finite(e.offset_y) || !finite(e.parallax_x) || !finite(e.parallax_y)) {
        return damaged("tile layer offset or parallax not finite");
    }
    if (!(e.opacity >= 0.0f && e.opacity <= 1.0f)) return damaged("tile layer opacity outside 0..1");
    const std::uint32_t layer_bytes = 2u * h.width * h.height;
    if (e.cells != h.offsets[kCells] + static_cast<std::uint32_t>(index) * layer_bytes) {
        return damaged("tile layer cells at the wrong offset");
    }
    if ((e.tilesets_used >> h.tilesets) != 0) return damaged("tile layer tilesets mask names a missing tileset");
    unsigned used = 0;
    const MapCheck cells = scan_cells(r, e.cells, used);
    if (!cells.ok()) return cells;
    if ((used & ~static_cast<unsigned>(e.tilesets_used)) != 0) {
        return damaged("tile layer tilesets mask misses a tileset its cells use");
    }
    return {};
}

MapCheck check_object(const Reader& r, const ObjectEntry& o) {
    if (!r.string_ok(o.name) || !r.string_ok(o.type)) return damaged("object name or type outside the string pool");
    if (o.object_layer >= r.h.object_layers) return damaged("object on an unknown object layer");
    if (o.shape > static_cast<std::uint8_t>(ObjectShape::Point)) return damaged("unknown object shape");
    if (o.id == 0 || o.id > 0x7FFFFFFFu) return damaged("object id outside 1..2^31-1");
    if (!finite(o.x) || !finite(o.y) || !finite(o.w) || !finite(o.h) || o.w < 0 || o.h < 0) {
        return damaged("object bounds negative or not finite");
    }
    if (o.shape == static_cast<std::uint8_t>(ObjectShape::Point) && (o.w != 0 || o.h != 0)) {
        return damaged("point object with a size");
    }
    return {};
}

/// Marks properties [first, first + count) as owned. Returns a problem (outside, or already owned) or null.
const char* claim(std::uint32_t* owned, int properties, int first, int count) {
    if (count == 0) return nullptr;
    if (first + count > properties) return "property range outside the properties";
    for (int i = first; i < first + count; ++i) {
        const std::uint32_t bit = 1u << (i % 32);
        if ((owned[i / 32] & bit) != 0) return "property ranges overlap";
        owned[i / 32] |= bit;
    }
    return nullptr;
}

/// Every owner's range lies inside the properties, no two overlap, and every property has an owner.
const char* ranges_problem(const Reader& r) {
    const Header& h = r.h;
    std::uint32_t owned[kMaxProperties / 32] = {};
    const int n = h.properties;
    const char* problem = claim(owned, n, h.properties_first, h.properties_count);
    for (int i = 0; !problem && i < h.tile_layers; ++i) {
        const auto e = r.at<TileLayerEntry>(kTileLayers, i);
        problem = claim(owned, n, e.properties_first, e.properties_count);
    }
    for (int i = 0; !problem && i < h.object_layers; ++i) {
        const auto e = r.at<ObjectLayerEntry>(kObjectLayers, i);
        problem = claim(owned, n, e.properties_first, e.properties_count);
    }
    for (int i = 0; !problem && i < h.objects; ++i) {
        const auto e = r.at<ObjectEntry>(kObjects, i);
        problem = claim(owned, n, e.properties_first, e.properties_count);
    }
    for (int i = 0; !problem && i < h.tile_props; ++i) {
        const auto e = r.at<TilePropsEntry>(kTileProps, i);
        problem = claim(owned, n, e.properties_first, e.properties_count);
    }
    for (int i = 0; !problem && i < n; ++i) {
        if (((owned[i / 32] >> (i % 32)) & 1u) == 0) problem = "property without an owner";
    }
    return problem;
}

} // namespace

MapCheck check_tile_layers(const Reader& r) {
    for (int i = 0; i < r.h.tile_layers; ++i) {
        const MapCheck c = check_layer(r, i);
        if (!c.ok()) return c;
    }
    return {};
}

MapCheck check_objects(const Reader& r) {
    const Header& h = r.h;
    int per_layer[kMaxObjectLayers] = {};
    for (int i = 0; i < h.objects; ++i) {
        const ObjectEntry o = r.at<ObjectEntry>(kObjects, i);
        const MapCheck c = check_object(r, o);
        if (!c.ok()) return c;
        ++per_layer[o.object_layer];
    }
    for (int i = 0; i < h.object_layers; ++i) {
        const ObjectLayerEntry e = r.at<ObjectLayerEntry>(kObjectLayers, i);
        if (!r.string_ok(e.name)) return damaged("object layer name: string offset outside the pool");
        if (e.object_count != per_layer[i]) return damaged("object layer counts do not match the objects");
    }
    return {};
}

MapCheck check_properties(const Reader& r) {
    for (int i = 0; i < r.h.properties; ++i) {
        const PropertyEntry p = r.at<PropertyEntry>(kProperties, i);
        if (p.name == 0) return damaged("property without a name");
        if (!r.string_ok(p.name)) return damaged("property name: string offset outside the pool");
        const auto type = static_cast<PropertyType>(p.type);
        if (type == PropertyType::None || p.type > static_cast<std::uint8_t>(PropertyType::String)) {
            return damaged("unknown property type");
        }
        if (type == PropertyType::Bool && p.value > 1) return damaged("bool property not 0 or 1");
        if (type == PropertyType::String && !r.string_ok(p.value)) {
            return damaged("property string value outside the string pool");
        }
    }
    const char* problem = ranges_problem(r);
    return problem ? damaged(problem) : MapCheck{};
}

} // namespace nnh::detail::mapcheck
