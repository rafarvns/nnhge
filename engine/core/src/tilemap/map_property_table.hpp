#pragma once
// The property section of a map block: every owner (map, tile layer, object layer, tile, object) has one
// contiguous range {first, count}; inserting a property shifts later entries and the ranges after it, so
// ranges stay contiguous whatever order properties are added in. Tiles with properties are a sparse table
// sorted by tile id. Pure (shared with tools/assetc). No allocation: callers check capacities first.

#include <cstdint>
#include <cstring>

#include "tilemap/map_layout.hpp"

namespace nnh::detail {

/// Points at an owner's range fields inside the block.
struct PropertyRange {
    std::uint16_t* first = nullptr;
    std::uint16_t* count = nullptr;
};

/// Index of the sparse-table entry of `tile_id`, or -1.
int find_tile_props(const MapView& map, int tile_id);

/// Adds an entry for `tile_id` (not present yet) in id order, with no properties; marks the tile's flag.
/// Needs header.tile_props < tile_props_cap. Returns its index.
int add_tile_props(const MapView& map, int tile_id);

/// Index of the property called `name` among properties [first, first + count), or -1. Inline, with the first bytes
/// compared before strcmp (plan 021, optimisation round: most names differ there, and a getter no longer pays two
/// calls and a strcmp per property).
inline int find_property(const MapView& map, int first, int count, const char* name) {
    const mapfmt::PropertyEntry* props = map.properties();
    const char* strings = map.strings();
    const char c = name[0];
    for (int i = first; i < first + count; ++i) {
        const char* s = strings + props[i].name;
        if (s[0] == c && (c == '\0' || std::strcmp(s + 1, name + 1) == 0)) return i;
    }
    return -1;
}

/// Inserts a zeroed property at the end of `range` and returns its index. Needs
/// header.properties < properties_cap. Every other range that starts at or after it moves up by one.
int insert_property(const MapView& map, PropertyRange range);

} // namespace nnh::detail
