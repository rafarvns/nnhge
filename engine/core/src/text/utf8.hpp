#pragma once
// UTF-8 decoding for text, and a bounded validity check (utf8_valid.cpp, used by the data validator). Pure; unit
// tested in tests/text/test_utf8.cpp.

#include <cstddef>

namespace nnh::detail {

/// Shown for invalid UTF-8 (U+FFFD).
inline constexpr char32_t kReplacementChar = 0xFFFD;

/// Decodes the code point at `p` and moves `p` past it. Returns 0 at the end of the string (and leaves
/// `p` there). Invalid, overlong, surrogate or truncated sequences give kReplacementChar and advance
/// at least one byte, never past the terminating '\0'.
char32_t next_codepoint(const char*& p);

/// True when the `size` bytes at `text` are well-formed UTF-8 (no overlong forms, surrogates, code points
/// above U+10FFFF or sequences cut by the end). NUL bytes count as valid characters. Reads only [text, text +
/// size).
bool utf8_valid(const char* text, std::size_t size);

} // namespace nnh::detail
