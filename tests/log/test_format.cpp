// Log line formatting (log/format.hpp).

#include <cstring>

#include "log/format.hpp"
#include "test.hpp"

using nnh::LogLevel;
using nnh::detail::LogLine;

namespace {

LogLine format(LogLevel level, const char* channel, const char* message) {
    LogLine line;
    nnh::detail::format_line(line, nnh::detail::LineHeader{1234, nnh::Duration::from_ms(20571), level, channel},
                             "%s", message);
    return line;
}

} // namespace

NNH_TEST(line_has_frame_time_level_channel_and_message) {
    const LogLine line = format(LogLevel::Warn, "gfx", "texture too big");
    CHECK(std::strcmp(line.text, "[f1234    20.571s] W gfx    texture too big") == 0);
}

NNH_TEST(each_level_has_its_letter) {
    CHECK(std::strstr(format(LogLevel::Trace, "a", "x").text, "] T a") != nullptr);
    CHECK(std::strstr(format(LogLevel::Debug, "a", "x").text, "] D a") != nullptr);
    CHECK(std::strstr(format(LogLevel::Info, "a", "x").text, "] I a") != nullptr);
    CHECK(std::strstr(format(LogLevel::Error, "a", "x").text, "] E a") != nullptr);
    CHECK(std::strstr(format(LogLevel::Fatal, "a", "x").text, "] F a") != nullptr);
}

NNH_TEST(long_messages_are_truncated_with_an_ellipsis) {
    char big[400];
    std::memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    const LogLine line = format(LogLevel::Info, "app", big);
    const std::size_t len = std::strlen(line.text);
    CHECK(len == nnh::log::kMaxLineLength - 1);
    CHECK(std::strcmp(line.text + len - 3, "...") == 0);
}

NNH_TEST(format_arguments_are_applied) {
    LogLine line;
    nnh::detail::format_line(line, nnh::detail::LineHeader{1, nnh::Duration{}, LogLevel::Info, "app"},
                             "x=%d name=%s", 42, "hero");
    CHECK(std::strstr(line.text, "x=42 name=hero") != nullptr);
}
