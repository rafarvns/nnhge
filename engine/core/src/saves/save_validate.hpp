#pragma once
// The schema-free structural check of a structured save's fields (spec 027, D45; constitution X): run on the
// whole payload before anything is written to the game's struct. Bounded: every varint canonical and in
// range, ids 1..65535 strictly ascending in each struct and template, wire bytes valid, depth <= 8, every
// length and count checked against the bytes left before use, a struct's fields filling exactly its length, a
// sequence's `bytes` exactly its items, strings valid UTF-8 without NUL, templates of 1..128 fields; linear
// in the size. Tested in tests/saves/test_validate.cpp, fuzzed by tests/fuzz/fuzz_saves.cpp.

#include "nnh/result.hpp"

#include <cstddef>
#include <cstdint>

namespace nnh::detail {

/// Checks the root fields [data, data + size) (the payload after the structured header). Errors: Corrupted
/// with a reason naming the field's path ("field 7: list count 900 exceeds the file", "field 9[12].4: string
/// is not UTF-8"). `steps` (optional) receives the number of values and template entries visited (the
/// linear-time test).
Result<void> save_validate_fields(const std::uint8_t* data, std::size_t size, std::uint32_t* steps = nullptr);

} // namespace nnh::detail
