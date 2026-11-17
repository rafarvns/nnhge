#pragma once
// Log line formatting. Pure; unit tested in tests/log/test_format.cpp.

#include <cstdarg>
#include <cstdint>

#include "nnh/log.hpp"
#include "nnh/time.hpp"

namespace nnh::detail {

struct LogLine {
    char text[log::kMaxLineLength] = {};
};

struct LineHeader {
    std::uint64_t frame;
    Duration time;
    LogLevel level;
    const char* channel;
};

/// "[f<frame> <seconds>s] <L> <channel> <message>", truncated to fit with "..." at the end.
void format_line_v(LogLine& out, const LineHeader& header, const char* format, std::va_list args);

void format_line(LogLine& out, const LineHeader& header, const char* format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

} // namespace nnh::detail
