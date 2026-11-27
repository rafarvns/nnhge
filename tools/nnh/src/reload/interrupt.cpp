// Ctrl+C while a script has swapped sources (see interrupt.hpp).

#include "reload/interrupt.hpp"

#include <atomic>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <csignal>
#endif

namespace nnh::tool::reload {
namespace {

std::atomic<bool> g_requested{false};
std::atomic<bool> g_handled{false};

#ifdef _WIN32
BOOL WINAPI on_console(DWORD event) {
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT && event != CTRL_CLOSE_EVENT) return FALSE;
    g_requested = true;
    if (event == CTRL_CLOSE_EVENT) { // the process ends when this returns: give the main thread time to restore
        for (int i = 0; i < 90 && !g_handled; ++i) Sleep(50);
    }
    return TRUE;
}
#else
extern "C" void on_signal(int) { g_requested = true; } // lock-free atomic store: async-signal-safe
#endif

} // namespace

void install_interrupt() {
    g_requested = false;
    g_handled = false;
#ifdef _WIN32
    SetConsoleCtrlHandler(on_console, TRUE);
#else
    std::signal(SIGINT, on_signal);
#endif
}

void uninstall_interrupt() {
#ifdef _WIN32
    SetConsoleCtrlHandler(on_console, FALSE);
#else
    std::signal(SIGINT, SIG_DFL);
#endif
}

bool interrupt_requested() { return g_requested; }

void interrupt_handled() { g_handled = true; }

void simulate_interrupt() { g_requested = true; }

} // namespace nnh::tool::reload
