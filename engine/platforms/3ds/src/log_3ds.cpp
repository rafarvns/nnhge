// Nintendo 3DS platform: log lines to stdout (redirected to the PC by 3dslink when available, see
// app_3ds.cpp) and, in Debug builds, to an attached debugger (contract nnh/platform/log.hpp).

#include <3ds.h>

#include <cstdio>
#include <cstring>

#include "nnh/build.hpp"
#include "nnh/platform/log.hpp"

namespace nnh::platform {

void log_console(LogLevel level, const char* line) {
    std::printf("%s\n", line);
    if (level >= LogLevel::Warn) std::fflush(stdout);
    if constexpr (build::flavor() == build::Flavor::Debug) {
        svcOutputDebugString(line, static_cast<s32>(std::strlen(line)));
    }
}

} // namespace nnh::platform
