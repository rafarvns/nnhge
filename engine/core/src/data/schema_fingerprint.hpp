#pragma once
// Schema fingerprint of a data type (spec 014 FR-10, D29): CRC-32 of a canonical encoding of the type and
// what it reaches — key, then per field in table order: name, kind, shape, count, capacity, integer width and
// sign, the effective range of numbers, `required`, and the nested type's encoding, the enum's names or the
// referenced type's key. Not covered (the converted data doesn't depend on them): member order, offsets,
// sizes, default values, C++ type names, enum values and widths, sources. Pure; the converter computes it
// from the description it reads, the console from the compiled tables, so they agree by construction. Never
// allocates; bounded for any registry (nested types are memoized per call and cut off below data::kMaxDepth +
// 1 levels, so cycles stop). Tests: tests/data_assets/test_fingerprint.cpp.

#include <cstdint>

#include "data/schema_registry.hpp"

namespace nnh::detail {

std::uint32_t schema_fingerprint(const DataRegistry& registry, const TypeInfo& type);

} // namespace nnh::detail
