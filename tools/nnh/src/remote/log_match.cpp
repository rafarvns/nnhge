// Log matching for scripts (see log_match.hpp).

#include "remote/log_match.hpp"

#include <vector>

#include "log_view.hpp"

namespace nnh::tool::remote {
namespace {

enum class Kind : std::uint8_t { Char, One, Any };

struct Token {
    Kind kind = Kind::Char;
    char c = 0;
};

/// The pattern as tokens, wrapped in `*` on both sides ("matches anywhere").
std::vector<Token> tokens_of(std::string_view pattern) {
    std::vector<Token> out;
    out.reserve(pattern.size() + 2);
    out.push_back({Kind::Any, 0});
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        const char c = pattern[i];
        if (c == '\\' && i + 1 < pattern.size() &&
            (pattern[i + 1] == '*' || pattern[i + 1] == '?' || pattern[i + 1] == '\\')) {
            out.push_back({Kind::Char, pattern[++i]});
        } else if (c == '*') {
            out.push_back({Kind::Any, 0});
        } else if (c == '?') {
            out.push_back({Kind::One, 0});
        } else {
            out.push_back({Kind::Char, c});
        }
    }
    out.push_back({Kind::Any, 0});
    return out;
}

} // namespace

bool log_matches(std::string_view pattern, std::string_view line) {
    const std::vector<Token> p = tokens_of(pattern);
    // Greedy match with backtracking to the last `*` (linear memory, O(pattern × line) at worst).
    std::size_t t = 0, s = 0;
    std::size_t star = p.size(), star_s = 0;
    while (s < line.size()) {
        if (t < p.size() && p[t].kind == Kind::Any) {
            star = t++;
            star_s = s;
        } else if (t < p.size() && (p[t].kind == Kind::One || p[t].c == line[s])) {
            ++t;
            ++s;
        } else if (star != p.size()) {
            t = star + 1;
            s = ++star_s;
        } else {
            return false;
        }
    }
    while (t < p.size() && p[t].kind == Kind::Any) ++t;
    return t == p.size();
}

LineInfo line_info(const std::string& line) {
    LineInfo info;
    const auto parsed = parse_log_line(line);
    if (!parsed) return info;
    // stamp = "f<digits> <seconds>s"
    std::uint64_t frame = 0;
    std::size_t i = 1;
    for (; i < parsed->stamp.size() && parsed->stamp[i] >= '0' && parsed->stamp[i] <= '9'; ++i) {
        frame = frame * 10 + static_cast<std::uint64_t>(parsed->stamp[i] - '0');
        if (frame > 0xFFFFFFFFu) return info;
    }
    if (i == 1 || i >= parsed->stamp.size() || parsed->stamp[i] != ' ') return info;
    info.has_frame = true;
    info.frame = static_cast<std::uint32_t>(frame);
    info.level = parsed->level;
    return info;
}

} // namespace nnh::tool::remote
