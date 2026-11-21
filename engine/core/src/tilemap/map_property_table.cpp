// Property ranges and the tile-properties sparse table of a map block (see map_property_table.hpp).

#include "tilemap/map_property_table.hpp"

#include <cstring>

namespace nnh::detail {

namespace {

using namespace mapfmt;

/// Moves the start of a non-empty range that begins at or after `pos` up by one.
void bump(std::uint16_t& first, std::uint16_t count, int pos) {
    if (count > 0 && first >= pos) first = static_cast<std::uint16_t>(first + 1);
}

void bump_all(const MapView& v, int pos) {
    Header& h = v.header();
    bump(h.properties_first, h.properties_count, pos);
    for (int i = 0; i < h.tile_layers; ++i) bump(v.tile_layers()[i].properties_first, v.tile_layers()[i].properties_count, pos);
    for (int i = 0; i < h.object_layers; ++i) {
        bump(v.object_layers()[i].properties_first, v.object_layers()[i].properties_count, pos);
    }
    for (int i = 0; i < h.objects; ++i) bump(v.objects()[i].properties_first, v.objects()[i].properties_count, pos);
    for (int i = 0; i < h.tile_props; ++i) bump(v.tile_props()[i].properties_first, v.tile_props()[i].properties_count, pos);
}

} // namespace

int find_tile_props(const MapView& v, int tile_id) {
    const TilePropsEntry* table = v.tile_props();
    for (int i = 0; i < v.header().tile_props; ++i) {
        if (table[i].tile_id == tile_id) return i;
    }
    return -1;
}

int add_tile_props(const MapView& v, int tile_id) {
    Header& h = v.header();
    TilePropsEntry* table = v.tile_props();
    int at = 0;
    while (at < h.tile_props && table[at].tile_id < tile_id) ++at;
    std::memmove(table + at + 1, table + at, (h.tile_props - at) * sizeof(TilePropsEntry));
    table[at] = {static_cast<std::uint16_t>(tile_id), 0, 0};
    h.tile_props = static_cast<std::uint16_t>(h.tile_props + 1);
    v.tile_flags()[tile_id] = static_cast<std::uint8_t>(v.tile_flags()[tile_id] | kTileHasProperties);
    return at;
}

int find_property(const MapView& v, int first, int count, const char* name) {
    const PropertyEntry* props = v.properties();
    for (int i = first; i < first + count; ++i) {
        if (std::strcmp(v.string(props[i].name), name) == 0) return i;
    }
    return -1;
}

int insert_property(const MapView& v, PropertyRange range) {
    Header& h = v.header();
    const int n = h.properties;
    const int pos = *range.count == 0 ? n : *range.first + *range.count;
    PropertyEntry* props = v.properties();
    std::memmove(props + pos + 1, props + pos, (n - pos) * sizeof(PropertyEntry));
    props[pos] = {};
    bump_all(v, pos); // our own range starts before pos (or is empty), so it does not move
    if (*range.count == 0) *range.first = static_cast<std::uint16_t>(pos);
    *range.count = static_cast<std::uint16_t>(*range.count + 1);
    h.properties = static_cast<std::uint16_t>(n + 1);
    return pos;
}

} // namespace nnh::detail
