#pragma once
// The schema description (spec 014 FR-3, D29): the text a game's schema host program writes from its registry
// and nnh-assetc reads (tools/assetc/src/schema_read.*). Pure; never allocates (text goes out piece by
// piece). Tests: tests/data_assets/test_schema_write.cpp.
//
// Format, version 1 — ASCII, LF, one record per line, tokens separated by one space; enums then types, each
// sorted by name, fields in table order (that order gives the field indices of converted files):
//
//   nnh-data-schema 1
//   enum <Name> <count> <name>...
//   type <Name> <key or -> <field count> <fingerprint, 8 hex digits> <schema source, to the end of the line>
//   field <name> <shape> <kind> [min <number>] [max <number>] [required]
//
// shape: `one`, `array <N>` or `list <N>`. kind: `bool`, `i8` `u8` `i16` `u16` `i32` `u32`, `float`,
// `string <capacity>`, `enum <Enum>`, `vec2`, `color`, `struct <Type>`, `ref <Type>`, `texture`, `sound`,
// `music`. min/max: the effective range, written only when narrower than the member type's (integers as
// integers, floats with 17 significant digits so they read back exactly). A name the registry can't resolve
// is written as `?` (the host program checks the schema before writing, so that never reaches a game build).

#include <cstddef>

#include "data/schema_registry.hpp"

namespace nnh::detail {

inline constexpr int kSchemaDescriptionVersion = 1;

/// Receives the description piece by piece.
using TextSink = void (*)(void* context, const char* text, std::size_t size);

void write_schema(const DataRegistry& registry, TextSink sink, void* context);

} // namespace nnh::detail
