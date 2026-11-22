// Bounded UTF-8 validity check (see utf8.hpp). Pure: shared with the host tools.

#include "text/utf8.hpp"

namespace nnh::detail {
namespace {

bool continuation(unsigned char b) { return (b & 0xC0) == 0x80; }

/// What a lead byte >= 0x80 starts — the sequence length (0 = can't start one) and the allowed
/// range of the second byte (which rules out overlong forms, surrogates and code points above U+10FFFF).
/// The same rules as next_codepoint (utf8.cpp), kept apart so games that only decode text link no extra code.
struct Lead {
    int length = 0;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;
};

Lead lead_of(unsigned char lead) {
    Lead l;
    if (lead >= 0xC2 && lead <= 0xDF) {
        l.length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        l.length = 3;
        if (lead == 0xE0) l.low = 0xA0;  // overlong
        if (lead == 0xED) l.high = 0x9F; // UTF-16 surrogates
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        l.length = 4;
        if (lead == 0xF0) l.low = 0x90;  // overlong
        if (lead == 0xF4) l.high = 0x8F; // above U+10FFFF
    }
    return l; // 0x80-0xC1 or 0xF5-0xFF can't start a character
}

bool fits(const Lead& l, int i, unsigned char b) {
    return i == 1 ? (b >= l.low && b <= l.high) : continuation(b);
}

} // namespace

bool utf8_valid(const char* text, std::size_t size) {
    const auto* s = reinterpret_cast<const unsigned char*>(text);
    std::size_t at = 0;
    while (at < size) {
        if (s[at] < 0x80) {
            ++at;
            continue;
        }
        const Lead l = lead_of(s[at]);
        if (l.length == 0 || size - at < static_cast<std::size_t>(l.length)) return false;
        for (int i = 1; i < l.length; ++i) {
            if (!fits(l, i, s[at + static_cast<std::size_t>(i)])) return false;
        }
        at += static_cast<std::size_t>(l.length);
    }
    return true;
}

} // namespace nnh::detail
