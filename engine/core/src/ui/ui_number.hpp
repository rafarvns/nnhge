#pragma once
// Integer text without printf (spec 028 FR-25): the same digits everywhere, no allocation. Pure; unit tested in
// tests/ui/test_number.cpp.

namespace nnh::detail {

/// Writes prefix, the signed decimal of `value` and suffix into `out`, at most `capacity` bytes (truncated; no NUL).
/// @return the bytes written.
int ui_format_number(char* out, int capacity, int value, const char* prefix = "", const char* suffix = "");

} // namespace nnh::detail
