// PC platform: log lines to the terminal, colored by level (contract nnh/platform/log.hpp); to nnh instead
// while `nnh run --pc` is connected (remote_pc.cpp).

#include <cstdio>

#include "nnh/build.hpp"
#include "nnh/platform/log.hpp"
#include "remote_pc.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace nnh::platform {
namespace {

bool g_checked = false;
bool g_colors = false;

bool detect_colors() {
#if defined(_WIN32)
    if (!_isatty(_fileno(stdout))) return false;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out == INVALID_HANDLE_VALUE || !GetConsoleMode(out, &mode)) return false;
    return SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

const char* color_for(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "\x1b[90m";
    case LogLevel::Debug: return "\x1b[36m";
    case LogLevel::Warn: return "\x1b[33m";
    case LogLevel::Error: return "\x1b[31m";
    case LogLevel::Fatal: return "\x1b[1;31m";
    default: return "";
    }
}

} // namespace

void log_console(LogLevel level, const char* line) {
    // Started by `nnh run --pc`: the line goes to nnh, which shows and saves it like a console's (spec 013).
    if constexpr (build::kDevToolsEnabled) {
        if (pc::remote_log_line(line)) return;
    }
    if (!g_checked) {
        g_colors = detect_colors();
        g_checked = true;
    }
    const char* color = g_colors ? color_for(level) : "";
    std::printf("%s%s%s\n", color, line, color[0] != '\0' ? "\x1b[0m" : "");
    if (level >= LogLevel::Warn) std::fflush(stdout);
}

} // namespace nnh::platform
