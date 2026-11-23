// Nintendo 3DS platform: CPU exception hook and crash screen (contract nnh/platform/crash.hpp).
//
// CPU exceptions reach threadOnException only if the kernel reports them to the process (UNITINFO
// non-zero, e.g. Luma3DS "Set developer UNITINFO"); otherwise Luma3DS shows its own exception screen and
// writes a dump to /luma/dumps/arm11/. Asserts and panics don't depend on this.

#include <3ds.h>

#include <sys/iosupport.h>

#include <cstdlib>
#include <cstring>

#include "nnh/platform/crash.hpp"

namespace nnh::platform {
namespace {

constexpr std::size_t kHandlerStackSize = 0x4000;
alignas(8) unsigned char g_handler_stack[kHandlerStackSize];
CpuFaultHandler g_core_handler = nullptr;

const char* exception_name(ERRF_ExceptionType type) {
    switch (type) {
    case ERRF_EXCEPTION_PREFETCH_ABORT: return "Prefetch abort";
    case ERRF_EXCEPTION_DATA_ABORT: return "Data abort";
    case ERRF_EXCEPTION_UNDEFINED: return "Undefined instruction";
    case ERRF_EXCEPTION_VFP: return "Floating point exception";
    }
    return "CPU exception";
}

[[noreturn]] void on_exception(ERRF_ExceptionInfo* info, CpuRegisters* regs) {
    CpuFault fault;
    fault.kind = exception_name(info->type);
    fault.has_registers = true;
    for (int r = 0; r < 13; ++r) fault.regs[r] = regs->r[r];
    fault.regs[13] = regs->sp;
    fault.regs[14] = regs->lr;
    fault.regs[15] = regs->pc;
    fault.cpsr = regs->cpsr;
    fault.has_fault_address = info->type == ERRF_EXCEPTION_DATA_ABORT;
    fault.fault_address = info->far;
    if (g_core_handler != nullptr) g_core_handler(fault);
    crash_show(fault.kind, "");
}

/// Writes to the text console consoleInit() selected, through its device: under 3dslink, stdout's descriptor
/// is dup2()ed onto the log socket (app_3ds.cpp), so printf would send the crash screen to the PC instead.
void console_write(const char* text) {
    const devoptab_t* console = devoptab_list[STD_OUT];
    if (console != nullptr && console->write_r != nullptr) console->write_r(_REENT, nullptr, text, std::strlen(text));
}

} // namespace

void crash_install_hooks(CpuFaultHandler handler) {
    g_core_handler = handler;
    threadOnException(on_exception, g_handler_stack + kHandlerStackSize, WRITE_DATA_TO_HANDLER_STACK);
}

void crash_show(const char* top, const char* bottom) {
    // Don't trust the GPU state: drop to libctru's text consoles on the CPU framebuffers.
    gfxExit();
    gfxInitDefault();
    PrintConsole top_console;
    PrintConsole bottom_console;
    consoleInit(GFX_TOP, &top_console);
    consoleInit(GFX_BOTTOM, &bottom_console);
    consoleSelect(&top_console);
    console_write("\x1b[41m\x1b[2J\x1b[1;1H"); // red background
    console_write(top);
    consoleSelect(&bottom_console);
    console_write("\x1b[2J\x1b[1;1H");
    console_write(bottom);

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    gfxExit();
    std::exit(EXIT_FAILURE);
}

bool debugger_attached() { return false; }
void debug_break() {}

} // namespace nnh::platform
