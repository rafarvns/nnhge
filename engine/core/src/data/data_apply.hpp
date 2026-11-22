#pragma once
// Applying a converted data asset to a struct (spec 014 FR-2, FR-10, FR-11): each value of a file
// validate_data accepted is written through the type's compiled offsets — integers at the member's width,
// floats by their bits, strings with the rest of their buffer zeroed (equal strings are equal bytes), enums
// through the game's value table, list counts into the List. Fields the file doesn't set are left as they
// are: the loader resets the struct to T{} first. Pure; never allocates. Tests:
// tests/data_assets/test_data_apply.cpp, test_data_layout.cpp.

#include <cstddef>

#include "data/schema_registry.hpp"

namespace nnh::detail {

/// Writes the values of the converted asset at `data` (validated by validate_data against the same `type` and
/// `registry`) into `out`, an object of `type`'s C++ type. Given an unvalidated file it still reads and
/// writes only inside the file and the object (list counts, string lengths and indices are clamped), but the
/// values are then meaningless.
void apply_data(const void* data, std::size_t size, const DataRegistry& registry, const TypeInfo& type,
                void* out);

} // namespace nnh::detail
