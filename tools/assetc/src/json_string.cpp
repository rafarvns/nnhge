// Decoding one JSON string literal (see json_string.hpp).

#include "json_string.hpp"

namespace nnh::assetc::json {

namespace {

struct Cursor {
    std::string_view text;
    std::size_t& pos;
    const char*& error;

    bool at_end() const { return pos >= text.size(); }
    unsigned byte(std::size_t at) const { return at < text.size() ? static_cast<unsigned char>(text[at]) : 0x100u; }
    bool fail(const char* what) {
        error = what;
        return false;
    }
};

void append_utf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

/// Four hex digits after "\u".
bool hex4(Cursor& c, unsigned& unit) {
    unit = 0;
    for (int i = 0; i < 4; ++i) {
        const unsigned b = c.byte(c.pos);
        unsigned digit;
        if (b >= '0' && b <= '9') digit = b - '0';
        else if (b >= 'a' && b <= 'f') digit = b - 'a' + 10;
        else if (b >= 'A' && b <= 'F') digit = b - 'A' + 10;
        else return c.fail("expected 4 hex digits after \\u");
        unit = unit * 16 + digit;
        ++c.pos;
    }
    return true;
}

/// "\uXXXX", or a surrogate pair "😀"; pos is just past the 'u'.
bool unicode_escape(Cursor& c, std::string& out) {
    unsigned unit = 0;
    if (!hex4(c, unit)) return false;
    if (unit >= 0xDC00 && unit <= 0xDFFF) return c.fail("lone low surrogate in a \\u escape");
    if (unit >= 0xD800 && unit <= 0xDBFF) {
        if (c.byte(c.pos) != '\\' || c.byte(c.pos + 1) != 'u') return c.fail("high surrogate without its low half");
        c.pos += 2;
        unsigned low = 0;
        if (!hex4(c, low)) return false;
        if (low < 0xDC00 || low > 0xDFFF) return c.fail("high surrogate without its low half");
        unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
    }
    append_utf8(out, unit);
    return true;
}

/// One escape; pos is at the backslash.
bool escape(Cursor& c, std::string& out) {
    ++c.pos;
    const unsigned b = c.byte(c.pos);
    const char* simple = "\"\\/bfnrt";
    const char* value = "\"\\/\b\f\n\r\t";
    for (int i = 0; simple[i] != '\0'; ++i) {
        if (b == static_cast<unsigned char>(simple[i])) {
            out += value[i];
            ++c.pos;
            return true;
        }
    }
    if (b != 'u') return c.fail(c.at_end() ? "unterminated string" : "unknown escape in a string");
    ++c.pos;
    return unicode_escape(c, out);
}

/// One multi-byte UTF-8 sequence: no overlong forms, no surrogates, at most U+10FFFF.
bool utf8_sequence(Cursor& c, std::string& out) {
    const unsigned lead = c.byte(c.pos);
    int length = 0;
    unsigned low = 0x80, high = 0xBF; // allowed range of the second byte
    if (lead >= 0xC2 && lead <= 0xDF) length = 2;
    else if (lead >= 0xE0 && lead <= 0xEF) length = 3;
    else if (lead >= 0xF0 && lead <= 0xF4) length = 4;
    else return c.fail("invalid UTF-8 in a string");
    if (lead == 0xE0) low = 0xA0;
    if (lead == 0xED) high = 0x9F;
    if (lead == 0xF0) low = 0x90;
    if (lead == 0xF4) high = 0x8F;
    for (int i = 1; i < length; ++i) {
        const unsigned b = c.byte(c.pos + static_cast<std::size_t>(i));
        const unsigned lo = i == 1 ? low : 0x80, hi = i == 1 ? high : 0xBF;
        if (b < lo || b > hi) return c.fail("invalid UTF-8 in a string");
    }
    out.append(c.text.substr(c.pos, static_cast<std::size_t>(length)));
    c.pos += static_cast<std::size_t>(length);
    return true;
}

} // namespace

bool decode_string(std::string_view text, std::size_t& pos, std::string& out, const char*& error) {
    Cursor c{text, pos, error};
    ++c.pos; // the opening quote
    for (;;) {
        if (c.at_end()) return c.fail("unterminated string");
        const unsigned b = c.byte(c.pos);
        if (b == '"') {
            ++c.pos;
            return true;
        }
        if (b < 0x20) return c.fail("control character in a string (escape it)");
        if (b == '\\') {
            if (!escape(c, out)) return false;
        } else if (b >= 0x80) {
            if (!utf8_sequence(c, out)) return false;
        } else {
            out += static_cast<char>(b);
            ++c.pos;
        }
    }
}

} // namespace nnh::assetc::json
