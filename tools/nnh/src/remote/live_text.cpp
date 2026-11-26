// Live values typed on the PC (see live_text.hpp).

#include "remote/live_text.hpp"

#include <charconv>
#include <cmath>
#include <cstring>

#include "live/live_format.hpp"
#include "live/live_steps.hpp"

namespace nnh::tool::remote {
namespace {

using detail::live::Kind;
namespace lv = detail::live;

std::string quote(const std::string& s) { return "\"" + s + "\""; }

bool parse_float(const std::string& t, float& out) {
    const char* b = t.data();
    const char* e = b + t.size();
    if (b != e && *b == '+') ++b;
    const auto r = std::from_chars(b, e, out);
    return r.ec == std::errc{} && r.ptr == e && !t.empty();
}

bool parse_int(const std::string& t, std::int64_t& out) {
    const char* b = t.data();
    const char* e = b + t.size();
    if (b != e && *b == '+') ++b;
    const auto r = std::from_chars(b, e, out);
    return r.ec == std::errc{} && r.ptr == e && !t.empty();
}

int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parse_color(const std::string& t, std::uint32_t& out) {
    if ((t.size() != 7 && t.size() != 9) || t[0] != '#') return false;
    std::uint8_t c[4] = {0, 0, 0, 255};
    for (std::size_t i = 0; i * 2 + 1 < t.size(); ++i) {
        const int hi = hex(t[1 + i * 2]), lo = hex(t[2 + i * 2]);
        if (hi < 0 || lo < 0) return false;
        c[i] = static_cast<std::uint8_t>(hi * 16 + lo);
    }
    out = lv::pack_rgba(c[0], c[1], c[2], c[3]);
    return true;
}

std::string float_text(float v) {
    char t[lv::kValueTextBytes];
    lv::format_float(v, t);
    return t;
}

const char* kind_word(Kind k) {
    switch (k) {
    case Kind::Float: return "a float";
    case Kind::Int32: return "an int";
    case Kind::Bool: return "a bool";
    case Kind::Color: return "a colour";
    case Kind::Enum: return "an enum";
    case Kind::Action: return "an action";
    default: return "a watch";
    }
}

std::string enum_list(const LiveItem& item) {
    std::string s;
    for (std::size_t i = 0; i < item.enum_names.size(); ++i) s += (i == 0 ? "" : ", ") + item.enum_names[i];
    return s;
}

/// Number kinds a watch may have, read as a double for comparisons.
double as_number(Kind k, const std::uint8_t* b) {
    std::uint32_t w = 0;
    std::memcpy(&w, b, 4);
    switch (k) {
    case Kind::Float: return lv::bits_float(w);
    case Kind::Int8:
    case Kind::Int16:
    case Kind::Int32: return static_cast<std::int32_t>(w);
    default: return w;
    }
}

bool numeric(Kind k) { return k != Kind::Text && k != Kind::Vec2 && k != Kind::Color && k != Kind::Bool; }

} // namespace

std::string unknown_name(const LiveCatalog& catalog, const std::string& name) {
    const std::string near = catalog.closest(name);
    return "unknown name " + quote(name) + (near.empty() ? "" : " (did you mean " + quote(near) + "?)");
}

std::string parse_set_value(const LiveItem& item, const std::string& text, std::uint32_t& bits) {
    const std::string who = item.name + " is " + kind_word(item.kind);
    switch (item.kind) {
    case Kind::Float: {
        float v = 0;
        if (!parse_float(text, v)) return who + ": " + quote(text) + " is not a number";
        if (!std::isfinite(v)) return item.name + ": " + text + " is not finite";
        if (v < lv::bits_float(item.min) || v > lv::bits_float(item.max)) {
            return item.name + ": " + text + " is outside " + float_text(lv::bits_float(item.min)) + ".." +
                   float_text(lv::bits_float(item.max));
        }
        bits = lv::float_bits(v);
        return "";
    }
    case Kind::Int32: {
        std::int64_t v = 0;
        if (!parse_int(text, v)) return who + ": " + quote(text) + " is not a whole number";
        const auto lo = static_cast<std::int32_t>(item.min), hi = static_cast<std::int32_t>(item.max);
        if (v < lo || v > hi) {
            return item.name + ": " + text + " is outside " + std::to_string(lo) + ".." + std::to_string(hi);
        }
        bits = static_cast<std::uint32_t>(static_cast<std::int32_t>(v));
        return "";
    }
    case Kind::Bool:
        if (text != "true" && text != "false") return who + ": use true or false";
        bits = text == "true" ? 1u : 0u;
        return "";
    case Kind::Color: return parse_color(text, bits) ? "" : who + ": use #RRGGBB or #RRGGBBAA";
    case Kind::Enum: {
        for (std::size_t i = 0; i < item.enum_names.size(); ++i) {
            if (item.enum_names[i] == text) {
                bits = static_cast<std::uint32_t>(i);
                return "";
            }
        }
        std::int64_t i = 0;
        if (parse_int(text, i) && i >= 0 && i < static_cast<std::int64_t>(item.enum_names.size())) {
            bits = static_cast<std::uint32_t>(i);
            return "";
        }
        return who + ": " + quote(text) + " is not one of " + enum_list(item);
    }
    case Kind::Action: return who + ": run it with `action " + item.name + "`";
    default: return who + ": it can't be set";
    }
}

std::string parse_expected(const LiveItem& item, const std::string& text, CompareOp op, Expected& out) {
    out = Expected{};
    out.kind = item.kind;
    const bool ordered = op != CompareOp::Eq && op != CompareOp::Ne;
    if (item.kind == Kind::Text) {
        if (ordered) return item.name + ": text is compared with == or != only";
        if (text.size() > 31) return item.name + ": text is at most 31 bytes";
        std::memcpy(out.bytes, text.data(), text.size());
        out.length = static_cast<int>(text.size());
        return "";
    }
    if (item.kind == Kind::Vec2) {
        if (ordered) return item.name + ": a Vec2 is compared with == or != only";
        std::string t = text;
        for (char& c : t) {
            if (c == '(' || c == ')' || c == ',') c = ' ';
        }
        const std::size_t a = t.find_first_not_of(' '), sp = t.find(' ', a), b = t.find_first_not_of(' ', sp);
        float xy[2] = {0, 0};
        if (a == std::string::npos || sp == std::string::npos || b == std::string::npos ||
            !parse_float(t.substr(a, sp - a), xy[0]) || !parse_float(t.substr(b, t.find(' ', b) - b), xy[1])) {
            return item.name + " is a Vec2: use (X, Y)";
        }
        std::memcpy(out.bytes, xy, 8);
        out.length = 8;
        return "";
    }
    if (ordered && !numeric(item.kind)) return item.name + ": only numbers and enums are compared with <, <=, > or >=";
    std::uint32_t bits = 0;
    if (item.watch()) { // any number the watch's kind can hold
        if (item.kind == Kind::Float) {
            float v = 0;
            if (!parse_float(text, v)) return item.name + " is a float: " + quote(text) + " is not a number";
            bits = lv::float_bits(v);
        } else if (item.kind == Kind::Bool) {
            if (text != "true" && text != "false") return item.name + " is a bool: use true or false";
            bits = text == "true" ? 1u : 0u;
        } else if (item.kind == Kind::Color) {
            if (!parse_color(text, bits)) return item.name + " is a colour: use #RRGGBB or #RRGGBBAA";
        } else {
            std::int64_t v = 0;
            if (!parse_int(text, v)) return item.name + " is an integer: " + quote(text) + " is not a whole number";
            bits = static_cast<std::uint32_t>(v);
        }
    } else {
        LiveItem wide = item; // a tunable: compare any value of its kind, not only those in its range
        wide.min = item.kind == Kind::Float ? lv::float_bits(-3.4e38f) : 0x80000000u;
        wide.max = item.kind == Kind::Float ? lv::float_bits(3.4e38f) : 0x7FFFFFFFu;
        const std::string e = parse_set_value(wide, text, bits);
        if (!e.empty()) return e;
    }
    std::memcpy(out.bytes, &bits, 4);
    out.length = 4;
    return "";
}

bool value_passes(const detail::remote::LiveValueRecord& v, CompareOp op, const Expected& e, double tolerance) {
    if (v.kind == Kind::Text || e.kind == Kind::Text) {
        const bool same = v.length == e.length && std::memcmp(v.value, e.bytes, static_cast<std::size_t>(e.length)) == 0;
        return op == CompareOp::Eq ? same : (op == CompareOp::Ne && !same);
    }
    if (v.kind == Kind::Vec2) {
        float a[2], b[2];
        std::memcpy(a, v.value, 8);
        std::memcpy(b, e.bytes, 8);
        const bool same = tolerance > 0 ? std::fabs(a[0] - b[0]) <= tolerance && std::fabs(a[1] - b[1]) <= tolerance
                                        : std::memcmp(a, b, 8) == 0;
        return op == CompareOp::Eq ? same : (op == CompareOp::Ne && !same);
    }
    const double a = as_number(v.kind, v.value), b = as_number(v.kind, e.bytes);
    const bool equal = tolerance > 0 ? std::fabs(a - b) <= tolerance
                                     : (v.kind == Kind::Float ? std::memcmp(v.value, e.bytes, 4) == 0 || a == b
                                                              : a == b);
    switch (op) {
    case CompareOp::Eq: return equal;
    case CompareOp::Ne: return !equal;
    case CompareOp::Lt: return a < b && !equal;
    case CompareOp::Le: return a <= b || equal;
    case CompareOp::Gt: return a > b && !equal;
    case CompareOp::Ge: return a >= b || equal;
    }
    return false;
}

std::string bits_text(const LiveItem& item, std::uint32_t bits) {
    if (item.kind == Kind::Enum) return bits < item.enum_names.size() ? item.enum_names[bits] : std::to_string(bits);
    char t[lv::kValueTextBytes];
    lv::format_bits(item.kind, bits, false, t);
    return t;
}

std::string value_text(const detail::remote::LiveValueRecord& v, const LiveItem* item) {
    if (v.kind == Kind::Vec2) {
        float xy[2];
        std::memcpy(xy, v.value, 8);
        char t[lv::kVec2TextBytes];
        lv::format_vec2(xy[0], xy[1], t);
        return t;
    }
    if (v.kind == Kind::Text) {
        char t[lv::kQuotedTextBytes];
        lv::format_text(reinterpret_cast<const char*>(v.value), v.length, t, sizeof t);
        return t;
    }
    std::uint32_t w = 0;
    std::memcpy(&w, v.value, 4);
    if (item != nullptr) return bits_text(*item, w);
    char t[lv::kValueTextBytes];
    lv::format_bits(v.kind, w, false, t);
    return t;
}

} // namespace nnh::tool::remote
