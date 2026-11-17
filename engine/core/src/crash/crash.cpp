// Failure path shared by asserts, panics and CPU faults: log fatal, build the report, show it.

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "core/engine_state.hpp"
#include "nnh/platform/fs.hpp"

#include "crash/crash_internal.hpp"
#include "crash/report.hpp"
#include "log/engine_channels.hpp"
#include "nnh/assert.hpp"
#include "nnh/build.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/crash.hpp"

namespace nnh {
namespace {

constexpr const char* kReportFile = "crash.txt";
constexpr const char* kPreviousReportFile = "crash.prev.txt";
constexpr int kScreenLogLines = 12; // what fits on the bottom screen below the header

bool g_crashing = false;

/// Writes the full report (every detail, all ring lines) to crash.txt, keeping the previous one.
/// Best effort: the crash screen is shown even if this fails. @return where the report went.
const char* write_report_file(const detail::CrashInfo& info) {
    if (!detail::engine_initialized()) return "the log console (engine not initialized)";
    static detail::CrashTexts details;
    detail::format_crash_texts(details, info, nullptr, 0, true, kReportFile);
    static char report[log::kRingLines * log::kMaxLineLength + 64];
    std::size_t used = static_cast<std::size_t>(
        std::snprintf(report, sizeof report, "NNHGE crash report (%s build)\n\n", build::flavor_name()));
    for (int i = 0; i < log::ring_size() && used + log::kMaxLineLength + 1 < sizeof report; ++i) {
        used += static_cast<std::size_t>(std::snprintf(report + used, sizeof report - used, "%s\n", log::ring_line(i)));
    }
    const platform::FsChunk chunks[3] = {{details.top, std::strlen(details.top)}, {"\nLog:\n", 6}, {report, used}};
    platform::fs_remove(kPreviousReportFile);
    platform::fs_rename(kReportFile, kPreviousReportFile);
    static char where[160];
    std::snprintf(where, sizeof where, "%s%s", platform::fs_root(), kReportFile);
    return platform::fs_write(kReportFile, chunks, 3) ? where : "the log console (writing crash.txt failed)";
}

const char* kind_name(detail::FailKind kind) {
    switch (kind) {
    case detail::FailKind::Assert: return "Assertion failed";
    case detail::FailKind::Verify: return "Verify failed";
    case detail::FailKind::Panic: return "Panic";
    }
    return "Failure";
}

[[noreturn]] void crash(const detail::CrashInfo& info) {
    if (g_crashing) platform::crash_show("Crash while handling a crash.", ""); // no recursion
    g_crashing = true;
    NNH_LOG_FATAL(app, "%s: %s (%s:%d)", info.kind, info.message, info.file != nullptr ? info.file : "?",
                  info.line);

    const char* lines[kScreenLogLines];
    const int total = log::ring_size();
    const int first = total > kScreenLogLines ? total - kScreenLogLines : 0;
    int count = 0;
    for (int i = first; i < total; ++i) lines[count++] = log::ring_line(i);

    const char* report_location = write_report_file(info);
    static detail::CrashTexts texts; // static: keep it off a possibly tiny fault stack
    detail::format_crash_texts(texts, info, lines, count, build::kDevToolsEnabled, report_location);
    platform::crash_show(texts.top, texts.bottom);
}

[[noreturn]] void on_cpu_fault(const platform::CpuFault& fault) {
    detail::CrashInfo info;
    info.kind = fault.kind;
    info.message = "The CPU stopped on an invalid operation (bad pointer, misaligned access...).";
    info.cpu = fault;
    crash(info);
}

} // namespace

void detail::fail(FailKind kind, const char* expression, const char* file, int line, const char* function,
                  const char* format, ...) {
    static char message[256];
    std::va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof message, format, args);
    va_end(args);

    if (platform::debugger_attached()) platform::debug_break();

    CrashInfo info;
    info.kind = kind_name(kind);
    info.message = message[0] != '\0' ? message : "(no message)";
    info.expression = expression;
    info.file = file;
    info.line = line;
    info.function = function;
    crash(info);
}

void detail::crash_install() {
    g_crashing = false;
    platform::crash_install_hooks(on_cpu_fault);
}

} // namespace nnh
