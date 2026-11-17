// Fake platform: console output is recorded, not printed (contract nnh/platform/log.hpp).

#include <cstring>

#include "fake_internal.hpp"
#include "nnh/platform/log.hpp"

namespace nnh::platform {

void log_console(LogLevel, const char* line) {
    platform_fake::State& s = platform_fake::detail::mutable_state();
    ++s.console_lines;
    std::strncpy(s.last_console_line, line, sizeof s.last_console_line - 1);
    s.last_console_line[sizeof s.last_console_line - 1] = '\0';
}

} // namespace nnh::platform
