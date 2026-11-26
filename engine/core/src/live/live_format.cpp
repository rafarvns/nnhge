// The text of live values (see live_format.hpp). Floats take Ryu's shortest digits (third_party/ryu, `f2s_buffered_n`,
// which prints `d.dddE±x`) and lay them out again; everything else is integer code without a division by a variable.

#include "live/live_format.hpp"

#include "ryu/ryu.h"

namespace nnh::detail::live {
namespace {

/// Writes the decimal digits of `v` at `out`; returns their count.
int put_uint(std::uint32_t v, char* out) {
    char tmp[10];
    int n = 0;
    do {
        tmp[n++] = static_cast<char>('0' + v % 10u);
        v /= 10u;
    } while (v != 0);
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    return n;
}

int put(char* out, int at, const char* text) {
    while (*text != '\0') out[at++] = *text++;
    return at;
}

constexpr char kHex[] = "0123456789ABCDEF";

int put_hex2(char* out, int at, std::uint8_t v) {
    out[at] = kHex[v >> 4];
    out[at + 1] = kHex[v & 15];
    return at + 2;
}

/// Ryu's scientific text split into its digits (no dot) and its decimal exponent (of the first digit).
struct Digits {
    char d[10];
    int count = 0;
    int exponent = 0;
    bool negative = false;
};

Digits split_ryu(const char* s, int n) {
    Digits out;
    int i = 0;
    if (s[i] == '-') {
        out.negative = true;
        ++i;
    }
    while (i < n && s[i] != 'E') {
        if (s[i] != '.' && out.count < 10) out.d[out.count++] = s[i];
        ++i;
    }
    ++i; // 'E'
    bool neg_exp = false;
    if (i < n && s[i] == '-') {
        neg_exp = true;
        ++i;
    }
    int e = 0;
    while (i < n) e = e * 10 + (s[i++] - '0');
    out.exponent = neg_exp ? -e : e;
    return out;
}

int layout_plain(const Digits& g, char* out, int at) {
    if (g.exponent < 0) {
        at = put(out, at, "0.");
        for (int z = 0; z < -g.exponent - 1; ++z) out[at++] = '0';
        for (int i = 0; i < g.count; ++i) out[at++] = g.d[i];
        return at;
    }
    for (int i = 0; i < g.count || i <= g.exponent; ++i) {
        if (i == g.exponent + 1) out[at++] = '.';
        out[at++] = i < g.count ? g.d[i] : '0';
    }
    return at;
}

int layout_scientific(const Digits& g, char* out, int at) {
    out[at++] = g.d[0];
    if (g.count > 1) {
        out[at++] = '.';
        for (int i = 1; i < g.count; ++i) out[at++] = g.d[i];
    }
    out[at++] = 'e';
    out[at++] = g.exponent < 0 ? '-' : '+';
    const int e = g.exponent < 0 ? -g.exponent : g.exponent;
    if (e < 10) out[at++] = '0';
    return at + put_uint(static_cast<std::uint32_t>(e), out + at);
}

} // namespace

int format_float(float v, char out[kValueTextBytes]) {
    const std::uint32_t bits = float_bits(v);
    const bool negative = (bits >> 31) != 0;
    int at = 0;
    if ((bits & 0x7F800000u) == 0x7F800000u) {
        at = (bits & 0x007FFFFFu) != 0 ? put(out, 0, "nan") : put(out, 0, negative ? "-inf" : "inf");
        out[at] = '\0';
        return at;
    }
    char ryu[24];
    const Digits g = split_ryu(ryu, f2s_buffered_n(v, ryu));
    if (g.negative) out[at++] = '-';
    at = g.exponent >= -5 && g.exponent <= 8 ? layout_plain(g, out, at) : layout_scientific(g, out, at);
    out[at] = '\0';
    return at;
}

int format_int(std::int64_t v, char out[kValueTextBytes]) {
    int at = 0;
    std::uint32_t magnitude = static_cast<std::uint32_t>(v);
    if (v < 0) {
        out[at++] = '-';
        magnitude = static_cast<std::uint32_t>(0u - static_cast<std::uint32_t>(v)); // |v| <= 2^31 here
    }
    at += put_uint(magnitude, out + at);
    out[at] = '\0';
    return at;
}

int format_bool(bool v, char out[kValueTextBytes]) {
    const int at = put(out, 0, v ? "true" : "false");
    out[at] = '\0';
    return at;
}

int format_color(std::uint32_t rgba, bool short_when_opaque, char out[kValueTextBytes]) {
    int at = 0;
    out[at++] = '#';
    const int channels = short_when_opaque && channel(rgba, 3) == 255 ? 3 : 4;
    for (int c = 0; c < channels; ++c) at = put_hex2(out, at, channel(rgba, c));
    out[at] = '\0';
    return at;
}

int format_vec2(float x, float y, char out[kVec2TextBytes]) {
    int at = 0;
    out[at++] = '(';
    at += format_float(x, out + at);
    at = put(out, at, ", ");
    at += format_float(y, out + at);
    out[at++] = ')';
    out[at] = '\0';
    return at;
}

int format_text(const char* text, int length, char* out, int cap) {
    int at = 0;
    out[at++] = '"';
    for (int i = 0; i < length; ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        const bool plain = c >= 0x20 && c <= 0x7E && c != '"' && c != '\\';
        const int need = plain ? 1 : (c == '"' || c == '\\' ? 2 : 4);
        if (at + need + 2 > cap) break; // room for the closing quote and the NUL
        if (plain) {
            out[at++] = static_cast<char>(c);
        } else if (need == 2) {
            out[at++] = '\\';
            out[at++] = static_cast<char>(c);
        } else {
            at = put(out, at, "\\x");
            at = put_hex2(out, at, c);
        }
    }
    out[at++] = '"';
    out[at] = '\0';
    return at;
}

int format_bits(Kind kind, std::uint32_t bits, bool panel, char out[kValueTextBytes]) {
    switch (kind) {
    case Kind::Float: return format_float(bits_float(bits), out);
    case Kind::Bool: return format_bool(bits != 0, out);
    case Kind::Color: return format_color(bits, panel, out);
    case Kind::Int8: return format_int(static_cast<std::int8_t>(bits), out);
    case Kind::Int16: return format_int(static_cast<std::int16_t>(bits), out);
    case Kind::Int32: return format_int(static_cast<std::int32_t>(bits), out);
    case Kind::UInt8:
    case Kind::UInt16:
    case Kind::UInt32:
    case Kind::Enum: return format_int(static_cast<std::int64_t>(bits), out);
    default: out[0] = '\0'; return 0;
    }
}

} // namespace nnh::detail::live
