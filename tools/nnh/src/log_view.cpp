#include "log_view.hpp"

#include <cstring>

namespace nnh::tool {

namespace {

constexpr char kReset[] = "\x1b[0m";
constexpr char kDim[] = "\x1b[90m";
constexpr char kCyan[] = "\x1b[36m";

const char* level_colour(char level) {
    switch (level) {
    case 'T':
    case 'D': return "\x1b[90m";   // grey
    case 'I': return "\x1b[32m";   // green
    case 'W': return "\x1b[33m";   // yellow
    case 'E': return "\x1b[31m";   // red
    default: return "\x1b[1;31m";  // F: bold red
    }
}

} // namespace

std::optional<LogLine> parse_log_line(const std::string& line) {
    // "[f<frame> <seconds>s] <L> <channel> <message>"
    if (line.size() < 4 || line[0] != '[' || line[1] != 'f') return std::nullopt;
    const std::size_t close = line.find("s] ");
    if (close == std::string::npos) return std::nullopt;
    LogLine out;
    out.stamp = line.substr(1, close);                 // "f12   0.200s"
    std::size_t at = close + 3;
    if (at + 1 >= line.size() || std::strchr("TDIWEF", line[at]) == nullptr || line[at] == '\0' || line[at + 1] != ' ') {
        return std::nullopt;
    }
    out.level = line[at];
    at += 2;
    const std::size_t channel_end = line.find(' ', at);
    out.channel = line.substr(at, channel_end == std::string::npos ? std::string::npos : channel_end - at);
    if (out.channel.empty()) return std::nullopt;
    if (channel_end != std::string::npos) {
        const std::size_t message = line.find_first_not_of(' ', channel_end);
        if (message != std::string::npos) out.message = line.substr(message);
    }
    return out;
}

std::string render_log_line(const std::string& line, bool colour) {
    if (!colour) return line;
    const auto l = parse_log_line(line);
    if (!l) return line;
    std::string channel = l->channel;
    if (channel.size() < 6) channel.resize(6, ' ');
    const bool bad = l->level == 'E' || l->level == 'F';
    std::string out;
    out.reserve(line.size() + 32);
    out += kDim;
    out += '[';
    out += l->stamp;
    out += "] ";
    out += kReset;
    out += level_colour(l->level);
    out += l->level;
    out += kReset;
    out += ' ';
    out += kCyan;
    out += channel;
    out += kReset;
    out += ' ';
    if (bad) out += level_colour(l->level);
    out += l->message;
    if (bad) out += kReset;
    return out;
}

} // namespace nnh::tool
