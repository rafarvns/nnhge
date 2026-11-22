#pragma once
// Bounded validation of a `.nnhmap` file (spec 012 FR-1, FR-11; constitution X): after it accepts a file, every
// count, offset, string, tile id and property range in it is inside the file, so copying it into a map block
// and running any query on it stays in bounds. Pure (no engine includes): the converter (tools/assetc) checks the
// files it writes with the same code. Unit tested in tests/tilemap/test_map_validate.cpp; fuzzed by
// tests/fuzz (tilemap_target).
//
// What it checks, in order (the first problem wins): header (size, magic, version, block size, CRC), capacities
// against the format limits, section offsets against the layout of those capacities, counts against capacities,
// the string pool, tilesets and the tile table, tile layers and their cells, object layers and objects,
// properties and every owner's property range. Runtime fields (collision-layer mask, texture handles, the
// owned-texture bits) are ignored: the loader recomputes them. Cost: one pass over the file (CRC + cells).

#include <cstddef>
#include <cstdint>

namespace nnh::detail {

enum class MapProblem : std::uint8_t {
    None,
    Damaged,      ///< not a map, truncated, CRC mismatch or inconsistent content (ErrorCode::Corrupted)
    NewerVersion, ///< a format or a feature this engine does not read yet (ErrorCode::Unsupported)
    OverLimit,    ///< over a documented map limit (ErrorCode::LimitReached)
};

struct MapCheck {
    MapProblem problem = MapProblem::None;
    const char* reason = ""; ///< static text, "" when ok
    bool ok() const { return problem == MapProblem::None; }
};

/// Validates `size` bytes at `data` (any alignment; `data` may be null when `size` is 0). Never allocates; reads
/// only [data, data + size).
MapCheck validate_map(const void* data, std::size_t size);

} // namespace nnh::detail
