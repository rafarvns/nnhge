#pragma once
// Crash screen texts. Pure formatting; unit tested in tests/crash/test_report.cpp.

#include "nnh/platform/crash.hpp"

namespace nnh::detail {

struct CrashInfo {
    const char* kind = "Crash";        ///< "Assertion failed", "Verify failed", "Panic", "Data abort"...
    const char* message = "";
    const char* expression = nullptr;  ///< the failed condition, if any
    const char* file = nullptr;
    int line = 0;
    const char* function = nullptr;
    platform::CpuFault cpu{};
};

struct CrashTexts {
    char top[1536] = {};
    char bottom[2048] = {};
};

/// Fills the two screens' texts. `details` = Debug/Development (everything on screen); otherwise a
/// friendly message plus the location, with details only in the report file.
void format_crash_texts(CrashTexts& out, const CrashInfo& info, const char* const* log_lines, int log_count,
                        bool details, const char* report_path);

} // namespace nnh::detail
