#pragma once
// Checking a whole data schema (spec 014 FR-1, FR-14, D29): the rules one NNH_DATA_TYPE can't check at
// compile time because they span types — unique keys and names, nested structs, enums and referenced types
// declared, Refs to types with a key, nesting depth, key syntax and reserved keys, counts, name lengths. Run
// by the schema host program before it writes the description (a problem fails the build); nnh-assetc runs it
// on the description it reads. Pure; never allocates; bounded for any registry (a cycle of nested types, only
// possible in a damaged description, counts as too deep). Tests: tests/data_assets/test_schema_check.cpp.

#include "data/schema_registry.hpp"

namespace nnh::detail {

/// Receives one problem, "<schema source>: <reason>" (or "schema: <reason>" for counts).
using SchemaIssueSink = void (*)(void* context, const char* message);

/// Checks `registry`, reporting each problem to `sink` (may be null); returns how many there were.
int check_schema(const DataRegistry& registry, SchemaIssueSink sink, void* context);

} // namespace nnh::detail
