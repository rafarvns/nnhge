#pragma once
// UTF-8 decoding for text. Pure; unit tested in tests/text/test_utf8.cpp.

namespace nnh::detail {

/// Shown for invalid UTF-8 (U+FFFD).
inline constexpr char32_t kReplacementChar = 0xFFFD;

/// Decodes the code point at `p` and moves `p` past it. Returns 0 at the end of the string (and leaves
/// `p` there). Invalid, overlong, surrogate or truncated sequences give kReplacementChar and advance
/// at least one byte, never past the terminating '\0'.
char32_t next_codepoint(const char*& p);

} // namespace nnh::detail
