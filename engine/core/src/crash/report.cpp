// Crash screen texts (top: what and where; bottom: recent logs and what to do).

#include "crash/report.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace nnh::detail {
namespace {

/// Appends printf-style text to a fixed buffer; silently stops when full.
template <std::size_t N>
void append(char (&buffer)[N], const char* format, ...) {
    const std::size_t used = std::strlen(buffer);
    if (used + 1 >= N) return;
    std::va_list args;
    va_start(args, format);
    std::vsnprintf(buffer + used, N - used, format, args);
    va_end(args);
}

const char* base_name(const char* path) {
    if (path == nullptr) return "?";
    const char* name = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') name = p + 1;
    }
    return name;
}

void append_registers(CrashTexts& out, const platform::CpuFault& cpu) {
    static const char* const kNames[16] = {"r0", "r1", "r2",  "r3",  "r4",  "r5", "r6", "r7",
                                           "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc"};
    for (int r = 0; r < 16; ++r) append(out.top, "%3s %08X%s", kNames[r], cpu.regs[r], r % 3 == 2 ? "\n" : "  ");
    append(out.top, "cpsr %08X\n", cpu.cpsr);
    if (cpu.has_fault_address) append(out.top, "fault address %08X\n", cpu.fault_address);
}

} // namespace

void format_crash_texts(CrashTexts& out, const CrashInfo& info, const char* const* log_lines, int log_count,
                        bool details, const char* report_path) {
    out.top[0] = out.bottom[0] = '\0';
    if (details) {
        append(out.top, "%s\n\n%s\n\n", info.kind, info.message);
        if (info.expression != nullptr) append(out.top, "check: %s\n", info.expression);
        append(out.top, "at %s:%d\n", base_name(info.file), info.line);
        if (info.function != nullptr) append(out.top, "in %s()\n", info.function);
        if (info.cpu.has_registers) {
            append(out.top, "\n");
            append_registers(out, info.cpu);
        }
        append(out.bottom, "Last log lines:\n");
        for (int i = 0; i < log_count; ++i) append(out.bottom, "%s\n", log_lines[i]);
        append(out.bottom, "\n");
    } else {
        append(out.top, "The game stopped because of an error.\n\n%s:%d\n", base_name(info.file), info.line);
    }
    append(out.bottom, "Report saved to %s\nPress START to exit.\n", report_path);
}

} // namespace nnh::detail
