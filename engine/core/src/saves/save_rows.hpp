#pragma once
// Applying the rows of a struct sequence (spec 027 T10.3). The template is matched with the item struct's
// fields once — a plan of one action per column: copy, check, convert, unknown or retired — and every row is
// applied by its columns' actions, without apply_row's per-value merge, tag reading and path steps. The
// values, the report's counts and paths and the values visited are apply_row's; a template of more than
// kRowPlanColumns columns uses apply_row. Tested in tests/saves/test_rows.cpp (and by every decoding test).

#include "saves/save_apply.hpp"
#include "saves/save_cursor.hpp"

#include <cstdint>

namespace nnh::detail {

/// Columns a row plan holds (6 bytes each, on the stack, per level of struct rows being decoded).
inline constexpr std::uint32_t kRowPlanColumns = 16;

/// Applies the first `n` rows of sequence `s` to the items at `items` of struct-sequence field `f` (each item
/// already T{}).
void apply_rows(SaveApply& a, const SaveFieldInfo& f, std::uint8_t* items, const SaveSequence& s,
                std::uint32_t n);

} // namespace nnh::detail
