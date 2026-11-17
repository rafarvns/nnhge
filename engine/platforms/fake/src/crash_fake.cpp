// Fake platform: crash screen is recorded and, inside run_expecting_crash(), jumps back to the test
// instead of ending the process (contract nnh/platform/crash.hpp).

#include <csetjmp>
#include <cstdlib>
#include <cstring>

#include "fake_internal.hpp"
#include "nnh/platform/crash.hpp"

namespace nnh {
namespace {

std::jmp_buf g_return_point;
bool g_armed = false;
platform_fake::Crash g_crash;
platform::CpuFaultHandler g_handler = nullptr;

void copy(char* out, std::size_t size, const char* text) {
    std::strncpy(out, text, size - 1);
    out[size - 1] = '\0';
}

} // namespace

void platform::crash_install_hooks(CpuFaultHandler handler) { g_handler = handler; }

void platform::crash_show(const char* top, const char* bottom) {
    copy(g_crash.top, sizeof g_crash.top, top);
    copy(g_crash.bottom, sizeof g_crash.bottom, bottom);
    if (g_armed) std::longjmp(g_return_point, 1);
    std::abort(); // a crash outside run_expecting_crash() is a real test failure
}

bool platform::debugger_attached() { return false; }
void platform::debug_break() {}

bool platform_fake::run_expecting_crash(void (*fn)()) {
    g_crash = {};
    g_armed = true;
    if (setjmp(g_return_point) != 0) {
        g_armed = false;
        return true;
    }
    fn();
    g_armed = false;
    return false;
}

const platform_fake::Crash& platform_fake::last_crash() { return g_crash; }

void platform_fake::simulate_cpu_fault(const platform::CpuFault& fault) {
    if (g_handler != nullptr) g_handler(fault);
}

} // namespace nnh
