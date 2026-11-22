// TOML values (toml.hpp): booleans, decimal numbers, basic strings and inline arrays; everything else TOML
// has is refused with its own message.

#include <charconv>
#include <cstdint>
#include <cstdio>

#include "toml_cursor.hpp"

namespace nnh::assetc::toml {

namespace {

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool ends_token(char c) {
    return c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == ']' || c == '#' ||
           c == '[' || c == '{' || c == '"' || c == '\'' || c == '}';
}

/// Digits with '_' only between digits, from `at`; leading zeros allowed or not. Returns the end, or npos
/// with the reason in `why`.
std::size_t digits(std::string_view s, std::size_t at, bool leading_zero_ok, const char*& why) {
    const std::size_t start = at;
    while (at < s.size() && (is_digit(s[at]) || s[at] == '_')) {
        if (s[at] == '_' &&
            (at == start || !is_digit(s[at - 1]) || at + 1 >= s.size() || !is_digit(s[at + 1]))) {
            why = "'_' must be between digits";
            return std::string_view::npos;
        }
        ++at;
    }
    if (at == start) return std::string_view::npos;
    if (!leading_zero_ok && s[start] == '0' && at - start > 1) {
        why = "leading zeros aren't allowed";
        return std::string_view::npos;
    }
    return at;
}

/// Checks `token` against TOML's decimal integer and float grammar; tells which one it is.
bool number_grammar(std::string_view t, bool& is_float, const char*& why) {
    std::size_t at = (t[0] == '+' || t[0] == '-') ? 1 : 0;
    at = digits(t, at, false, why);
    if (at == std::string_view::npos) return false;
    is_float = false;
    if (at < t.size() && t[at] == '.') {
        at = digits(t, at + 1, true, why);
        if (at == std::string_view::npos) return false;
        is_float = true;
    }
    if (at < t.size() && (t[at] == 'e' || t[at] == 'E')) {
        ++at;
        if (at < t.size() && (t[at] == '+' || t[at] == '-')) ++at;
        at = digits(t, at, true, why);
        if (at == std::string_view::npos) return false;
        is_float = true;
    }
    return at == t.size();
}

bool read_number(Cursor& c, std::string_view t, Value& out) {
    const std::string_view bare = (t[0] == '+' || t[0] == '-') ? t.substr(1) : t;
    if (bare == "inf" || bare == "nan") return c.fail("inf and nan aren't supported");
    if (t.find(':') != std::string_view::npos ||
        (t.size() >= 5 && is_digit(t[0]) && is_digit(t[3]) && t[4] == '-')) {
        return c.fail("dates and times aren't supported");
    }
    if (bare.size() > 1 && bare[0] == '0' && (bare[1] == 'x' || bare[1] == 'o' || bare[1] == 'b')) {
        return c.fail("hexadecimal, octal and binary numbers aren't supported");
    }
    if (!is_digit(t[0]) && t[0] != '+' && t[0] != '-' && t[0] != '.') {
        return c.fail("text without quotes: strings are written \"" + std::string(t) + "\"");
    }
    bool is_float = false;
    const char* why = nullptr;
    if (!number_grammar(t, is_float, why)) {
        return c.fail(why != nullptr ? std::string(why) : "not a number: " + std::string(t));
    }
    std::string plain; // without '+' and '_', which from_chars doesn't take
    for (char ch : t) {
        if (ch != '_' && ch != '+') plain += ch;
    }
    out.text = std::string(t);
    out.type = is_float ? Type::Float : Type::Integer;
    const char* end = plain.data() + plain.size();
    const auto result = is_float ? std::from_chars(plain.data(), end, out.number)
                                 : std::from_chars(plain.data(), end, out.integer);
    out.out_of_range = result.ec == std::errc::result_out_of_range;
    return true;
}

bool hex4(Cursor& c, std::uint32_t& code) {
    code = 0;
    for (int i = 0; i < 4; ++i) {
        const char h = c.peek();
        const int v = is_digit(h)              ? h - '0'
                      : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                      : (h >= 'A' && h <= 'F') ? h - 'A' + 10
                                               : -1;
        if (v < 0) return c.fail("\\u needs 4 hexadecimal digits");
        code = code * 16 + static_cast<std::uint32_t>(v);
        ++c.at;
    }
    char text[16];
    std::snprintf(text, sizeof text, "\\u%04X", static_cast<unsigned>(code));
    if (code >= 0xD800 && code <= 0xDFFF) return c.fail(std::string(text) + " isn't a character");
    if (code == 0) return c.fail("\\u0000 isn't allowed");
    return true;
}

void append_utf8(std::string& out, std::uint32_t code) {
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

bool read_string(Cursor& c, Value& out) {
    if (c.peek(1) == '"' && c.peek(2) == '"') return c.fail("multi-line strings aren't supported");
    ++c.at;
    out.type = Type::String;
    for (;;) {
        const char ch = c.peek();
        if (c.done() || ch == '\n' || ch == '\r') return c.fail("unterminated string");
        ++c.at;
        if (ch == '"') return true;
        if (ch != '\\') {
            out.text += ch;
            continue;
        }
        const char e = c.peek();
        ++c.at;
        if (e == '"' || e == '\\')
            out.text += e;
        else if (e == 'n')
            out.text += '\n';
        else if (e == 't')
            out.text += '\t';
        else if (e == 'u') {
            std::uint32_t code = 0;
            if (!hex4(c, code)) return false;
            append_utf8(out.text, code);
        } else {
            return c.fail(std::string("escape \\") + (e >= ' ' ? e : '?') +
                          " isn't supported (use \\\" \\\\ \\n \\t or \\uXXXX)");
        }
    }
}

bool read_array(Cursor& c, Value& out, int depth) {
    if (depth + 1 > kMaxArrayDepth) return c.fail("arrays nested more than 2 deep");
    ++c.at;
    out.type = Type::Array;
    for (;;) {
        c.skip_blank();
        if (c.done()) return c.fail("unterminated array: expected ']'");
        if (c.peek() == ']') break;
        Value item;
        if (!read_value(c, item, depth + 1)) return false;
        out.items.push_back(std::move(item));
        c.skip_blank();
        if (c.peek() == ',') {
            ++c.at;
        } else if (c.done()) {
            return c.fail("unterminated array: expected ']'");
        } else if (c.peek() != ']') {
            return c.fail("expected ',' or ']' in the array");
        }
    }
    ++c.at;
    return true;
}

} // namespace

bool read_value(Cursor& c, Value& out, int depth) {
    out.line = c.line;
    const char first = c.peek();
    if (first == '"') return read_string(c, out);
    if (first == '[') return read_array(c, out, depth);
    if (first == '\'') return c.fail("literal strings ('...') aren't supported: use \"...\"");
    if (first == '{') return c.fail("inline tables ({...}) aren't supported: write a [section]");
    if (ends_token(first)) return c.fail("expected a value");
    std::size_t end = c.at;
    while (!ends_token(c.peek(end - c.at)))
        ++end;
    const std::string_view token = c.text.substr(c.at, end - c.at);
    c.at = end;
    if (token == "true" || token == "false") {
        out.type = Type::Bool;
        out.boolean = token == "true";
        return true;
    }
    return read_number(c, token, out);
}

} // namespace nnh::assetc::toml
