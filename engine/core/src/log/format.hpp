#pragma once
// Log line formatting. Pure; unit tested in tests/log/test_format.cpp.

#include <cstdarg>
#include <cstdint>

#include "nnh/log.hpp"
#include "nnh/time.hpp"
#include "nnh/detail/printf_format.hpp"

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

void format_line(LogLine& out, const LineHeader& header, const char* format, ...) NNH_PRINTF_FORMAT(3, 4);

} // namespace nnh::detail
