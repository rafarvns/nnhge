#pragma once
// Bounded validation of a converted data asset `NNHD` (spec 014 FR-10, FR-11, FR-14; constitution X) against
// a type's schema: after it accepts a file, every field index, value, string, enum index and list count in it
// fits the type, so apply_data stays in bounds and writes only values the schema allows. Pure: nnh-assetc
// checks the files it writes with the same code. Tests: tests/data_assets/test_data_validate.cpp; fuzzed by
// tests/fuzz (data_target).
//
// Order (the first problem wins): header (size, magic, version, flags, size field, file limit, CRC), the
// fingerprint, then one walk of the body: field indices strictly increasing and in range, required fields
// present in every struct value written, bools 0/1, integers and floats finite and inside the field's
// effective range, strings within their capacity without NUL and in UTF-8, enum indices below the name count,
// list counts within capacity, nesting within data::kMaxDepth, no bytes left over. Every value takes at least
// one byte, so the walk is linear in the file size.

#include <cstddef>
#include <cstdint>

#include "data/schema_registry.hpp"

namespace nnh::detail {

enum class DataProblem : std::uint8_t {
    None,
    Damaged,      ///< not a data asset, truncated, CRC mismatch or a value the schema forbids
                  ///< (ErrorCode::Corrupted)
    NewerVersion, ///< a format version or flag this engine doesn't read (ErrorCode::Unsupported)
    Stale,     ///< converted for another version of the type: the fingerprint differs (ErrorCode::Corrupted)
    OverLimit, ///< larger than datafmt::kMaxFileBytes (ErrorCode::LimitReached)
};

struct DataCheck {
    DataProblem problem = DataProblem::None;
    const char* reason = ""; ///< static text, "" when ok
    bool ok() const { return problem == DataProblem::None; }
};

/// Validates `size` bytes at `data` (any alignment; may be null when size is 0) as an asset of `type`, whose
/// schema fingerprint is `fingerprint` (schema_fingerprint(registry, type)). Nested types and enums are
/// looked up in `registry`. Never allocates; reads only [data, data + size).
DataCheck validate_data(const void* data, std::size_t size, const DataRegistry& registry,
                        const TypeInfo& type, std::uint32_t fingerprint);

} // namespace nnh::detail
