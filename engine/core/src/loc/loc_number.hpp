#pragma once
// Integers as a language writes them (spec 029 FR-18, FR-24): decimal digits, '-' for negatives, the group separator
// (UTF-8, e.g. ",", ".", NBSP) between groups of three when the number has at least `grouping_min` digits. Exact for
// INT64_MIN. Pure; no division by a variable, no printf. Tests: tests/loc/test_loc_number.cpp.

#include <cstdint>

namespace nnh::detail::loc {

/// Bytes the longest result takes: 20 digits, the sign, 6 separators of up to 4 bytes.
inline constexpr int kMaxNumberBytes = 48;

/// Writes `value` into `out` (at least kMaxNumberBytes, no NUL). @return the bytes written.
int format_integer(long long value, const char* grouping, int grouping_min, char* out);

} // namespace nnh::detail::loc
