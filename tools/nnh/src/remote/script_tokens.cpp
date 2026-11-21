// The words of one script line (see script_tokens.hpp).

#include "remote/script_tokens.hpp"

#include "remote/script.hpp"

namespace nnh::tool::remote {
namespace {

bool is_space(char c) { return c == ' ' || c == '\t'; }
bool is_digit(char c) { return c >= '0' && c <= '9'; }

/// Reads a quoted text starting at line[i] == '"'. @return "" or the error; `i` ends after the quote.
std::string read_quoted(std::string_view line, std::size_t& i, Token& token) {
    for (++i; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            ++i;
            if (i < line.size() && !is_space(line[i]) && line[i] != '#') {
                return "a closing quote must be followed by a space";
            }
            return "";
        }
        if (c == '\\' && i + 1 < line.size() && (line[i + 1] == '"' || line[i + 1] == '\\')) {
            token.text += line[++i];
        } else {
            token.text += c;
        }
    }
    return "unterminated quote";
}

/// Whole number of up to `max_digits` digits.
bool parse_whole(std::string_view s, std::size_t max_digits, std::uint32_t& out) {
    if (s.empty() || s.size() > max_digits) return false;
    out = 0;
    for (char c : s) {
        if (!is_digit(c)) return false;
        out = out * 10 + static_cast<std::uint32_t>(c - '0');
    }
    return true;
}

struct ButtonName {
    const char* name;
    std::uint32_t bit;
};

constexpr ButtonName kButtons[] = {
    {"a", 1u << 0},      {"b", 1u << 1},       {"x", 1u << 2},   {"y", 1u << 3},    {"l", 1u << 4},
    {"r", 1u << 5},      {"zl", kButtonZL},    {"zr", kButtonZR}, {"start", 1u << 8}, {"select", 1u << 9},
    {"up", 1u << 10},    {"down", 1u << 11},   {"left", 1u << 12}, {"right", 1u << 13},
};

} // namespace

std::string tokenize(std::string_view line, std::vector<Token>& out) {
    std::size_t i = 0;
    while (i < line.size()) {
        if (is_space(line[i])) {
            ++i;
        } else if (line[i] == '#') {
            break;
        } else if (line[i] == '"') {
            Token token{.text = {}, .quoted = true};
            const std::string error = read_quoted(line, i, token);
            if (!error.empty()) return error;
            out.push_back(std::move(token));
        } else {
            Token token;
            while (i < line.size() && !is_space(line[i]) && line[i] != '#') token.text += line[i++];
            out.push_back(std::move(token));
        }
    }
    return "";
}

bool parse_frames(std::string_view s, std::uint32_t& out) {
    return parse_whole(s, 7, out) && out >= 1 && out <= kMaxFrames;
}

bool parse_index(std::string_view s, int max, std::int16_t& out) {
    std::uint32_t v = 0;
    if (!parse_whole(s, 5, v) || v > static_cast<std::uint32_t>(max)) return false;
    out = static_cast<std::int16_t>(v);
    return true;
}

bool parse_stick_value(std::string_view s, std::int16_t& out) {
    const bool negative = !s.empty() && s[0] == '-';
    if (negative) s.remove_prefix(1);
    const std::size_t dot = s.find('.');
    const std::string_view whole = s.substr(0, dot);
    std::uint32_t units = 0, thousandths = 0;
    if (whole.size() > 1 || (!whole.empty() && !parse_whole(whole, 1, units))) return false;
    if (dot != std::string_view::npos) {
        std::string_view fraction = s.substr(dot + 1);
        if (!parse_whole(fraction, 3, thousandths)) return false; // 1 to 3 digits
        for (std::size_t n = fraction.size(); n < 3; ++n) thousandths *= 10;
    } else if (whole.empty()) {
        return false;
    }
    const std::uint32_t value = units * 1000 + thousandths;
    if (value > 1000) return false;
    out = static_cast<std::int16_t>(negative ? -static_cast<int>(value) : static_cast<int>(value));
    return true;
}

std::string parse_buttons(std::string_view s, std::uint32_t& out) {
    out = 0;
    for (;;) {
        const std::size_t plus = s.find('+');
        const std::string name = lower(s.substr(0, plus));
        bool known = false;
        for (const ButtonName& b : kButtons) {
            if (name == b.name) {
                out |= b.bit;
                known = true;
            }
        }
        if (!known) {
            return "unknown button '" + std::string(s.substr(0, plus)) +
                   "' (A B X Y L R ZL ZR START SELECT UP DOWN LEFT RIGHT)";
        }
        if (plus == std::string_view::npos) return "";
        s.remove_prefix(plus + 1);
    }
}

bool valid_name(std::string_view s) {
    if (s.empty() || s.size() > kMaxNameChars) return false;
    for (char c : s) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_digit(c) || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

} // namespace nnh::tool::remote
