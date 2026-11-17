// Log line formatting into a fixed buffer (no heap).

#include "log/format.hpp"

#include <cstdio>
#include <cstring>

namespace nnh::detail {
namespace {

char level_letter(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return 'T';
    case LogLevel::Debug: return 'D';
    case LogLevel::Info: return 'I';
    case LogLevel::Warn: return 'W';
    case LogLevel::Error: return 'E';
    case LogLevel::Fatal: return 'F';
    case LogLevel::Off: break;
    }
    return '?';
}

void mark_truncated(LogLine& out) {
    const std::size_t end = sizeof out.text - 1;
    out.text[end - 3] = '.';
    out.text[end - 2] = '.';
    out.text[end - 1] = '.';
    out.text[end] = '\0';
}

} // namespace

void format_line_v(LogLine& out, const LineHeader& header, const char* format, std::va_list args) {
    const std::int64_t ms = header.time.ms();
    const int prefix = std::snprintf(out.text, sizeof out.text, "[f%-6llu%4lld.%03llds] %c %-6s ",
                                     static_cast<unsigned long long>(header.frame),
                                     static_cast<long long>(ms / 1000), static_cast<long long>(ms % 1000),
                                     level_letter(header.level), header.channel);
    if (prefix < 0 || static_cast<std::size_t>(prefix) >= sizeof out.text) {
        mark_truncated(out);
        return;
    }
    const std::size_t room = sizeof out.text - static_cast<std::size_t>(prefix);
    const int written = std::vsnprintf(out.text + prefix, room, format, args);
    if (written >= 0 && static_cast<std::size_t>(written) >= room) mark_truncated(out);
}

void format_line(LogLine& out, const LineHeader& header, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    format_line_v(out, header, format, args);
    va_end(args);
}

} // namespace nnh::detail
