#pragma once
// A prefab library's block layout (spec 021 FR-1, plan 021 "Data and memory"): pure, from the capacities and whether
// names are kept (Debug/Development).

#include <cstddef>

#include "nnh/prefabs.hpp"

namespace nnh::detail {

struct LibraryLayout {
    std::size_t hashes = 0, table = 0, records = 0, names = 0, entries = 0, values = 0;
    std::size_t total = 0;
    int table_size = 0; ///< a power of two ≥ 2 × max_prefabs
};

/// Offsets of each part from the block's start (each aligned to its type; values to 16) and the total. The capacities
/// are assumed valid (PrefabLibrary::create checks them first).
LibraryLayout library_layout(const PrefabLibraryDesc& desc, bool names);

} // namespace nnh::detail
