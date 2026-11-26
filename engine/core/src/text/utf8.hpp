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

/// next_codepoint() with ASCII and the common well-formed 3-byte sequences (lead E1..EC, EE, EF: kana, CJK) decoded
/// inline — the per-glyph loops of game fonts and the wrapper (T10.3); everything else goes to next_codepoint().
inline char32_t next_codepoint_fast(const char*& p) {
    const auto* s = reinterpret_cast<const unsigned char*>(p);
    const unsigned lead = s[0];
    if (lead < 0x80) {
        if (lead != 0) ++p;
        return lead;
    }
    if ((lead >= 0xE1 && lead <= 0xEC) || lead == 0xEE || lead == 0xEF) {
        const unsigned b1 = s[1];
        if ((b1 & 0xC0u) == 0x80u) { // s[2] is readable: s[1] isn't the terminator
            const unsigned b2 = s[2];
            if ((b2 & 0xC0u) == 0x80u) {
                p += 3;
                return static_cast<char32_t>(((lead & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu));
            }
        }
    }
    return next_codepoint(p);
}

/// True when the `size` bytes at `text` are well-formed UTF-8 (no overlong forms, surrogates, code points
/// above U+10FFFF or sequences cut by the end). NUL bytes count as valid characters. Reads only [text, text +
/// size).
bool utf8_valid(const char* text, std::size_t size);

} // namespace nnh::detail
