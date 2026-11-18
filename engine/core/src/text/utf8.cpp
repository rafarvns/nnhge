// UTF-8 decoding (utf8.hpp). Follows the Unicode "maximal subpart" practice: a bad sequence becomes one
// replacement character and decoding resumes at the first byte that can't continue it.

#include "text/utf8.hpp"

namespace nnh::detail {
namespace {

bool continuation(unsigned char b) { return (b & 0xC0) == 0x80; }

} // namespace

char32_t next_codepoint(const char*& p) {
    const auto* s = reinterpret_cast<const unsigned char*>(p);
    const unsigned char lead = s[0];
    if (lead == 0) return 0;
    if (lead < 0x80) {
        ++p;
        return lead;
    }
    int length = 0;
    char32_t value = 0;
    unsigned char low = 0x80; // allowed range of the second byte (rules out overlong and surrogates)
    unsigned char high = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
        value = lead & 0x1F;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        value = lead & 0x0F;
        if (lead == 0xE0) low = 0xA0;  // overlong
        if (lead == 0xED) high = 0x9F; // UTF-16 surrogates
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        value = lead & 0x07;
        if (lead == 0xF0) low = 0x90;  // overlong
        if (lead == 0xF4) high = 0x8F; // above U+10FFFF
    } else {
        ++p; // 0x80-0xC1 or 0xF5-0xFF can't start a character
        return kReplacementChar;
    }
    for (int i = 1; i < length; ++i) {
        const unsigned char b = s[i];
        const bool ok = i == 1 ? (b >= low && b <= high) : continuation(b);
        if (!ok) { // also stops at '\0', which is never a continuation byte
            p += i;
            return kReplacementChar;
        }
        value = (value << 6) | (b & 0x3F);
    }
    p += length;
    return value;
}

} // namespace nnh::detail
