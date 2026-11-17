#pragma once
/// @file nnh/platform/crash.hpp
/// Platform contract — crash handling. Implemented by every platform library, called only by nnh::core.
/// Not for games. See docs/architecture/errors.md.

#include <cstdint>

namespace nnh::platform {

/// CPU state at a hardware fault (3DS: data/prefetch abort, undefined instruction; PC: fatal signal).
struct CpuFault {
    const char* kind = "CPU exception";
    bool has_registers = false;
    std::uint32_t regs[16] = {}; ///< r0..r12, sp, lr, pc
    std::uint32_t cpsr = 0;
    bool has_fault_address = false;
    std::uint32_t fault_address = 0;
};

/// Called by the platform when the CPU faults. Provided by core; never returns.
using CpuFaultHandler = void (*)(const CpuFault& fault);

/// Installs the system's fault hooks (3DS: threadOnException; PC: signal handlers) so faults reach
/// `handler`. Called by nnh::init() after init(). This callback is the only way a platform library
/// reaches core code (no link-time dependency).
void crash_install_hooks(CpuFaultHandler handler);

/// Shows the crash screen — `top` on the top screen, `bottom` on the bottom screen (plain text, '\n'
/// separated) — waits until the user dismisses it (START on 3DS, closing the window on PC) and ends the
/// program. Must work even if the renderer is broken or a frame is open. Never returns.
[[noreturn]] void crash_show(const char* top, const char* bottom);

/// True if a debugger is attached (PC: break into it on asserts instead of showing the crash screen).
bool debugger_attached();

/// Stops in the attached debugger. Only called when debugger_attached() is true.
void debug_break();

} // namespace nnh::platform
