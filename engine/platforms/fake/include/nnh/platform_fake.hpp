#pragma once
/// @file nnh/platform_fake.hpp
/// Controls and recordings of the fake platform (nnh::platform_fake), used by unit and conformance
/// tests to drive nnh::core without hardware. Not available to games.

#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/crash.hpp"
#include "nnh/time.hpp"

namespace nnh::platform_fake {

inline constexpr int kMaxRecordedTexts = 32;
inline constexpr int kMaxTextLength = 64;

/// One recorded debug_text() call (text truncated to kMaxTextLength - 1 characters).
struct DrawnText {
    Screen screen = Screen::Top;
    int x = 0;
    int y = 0;
    Color color{};
    char text[kMaxTextLength] = {};
};

/// Everything the fake platform recorded since the last reset().
struct State {
    int init_calls = 0;
    int shutdown_calls = 0;
    int frames_begun = 0;
    int frames_ended = 0;
    bool initialized = false;
    bool frame_open = false;
    Color top_clear{};
    Color bottom_clear{};
    int text_count = 0; ///< texts drawn in the current frame (cleared by begin_frame)
    DrawnText texts[kMaxRecordedTexts];
    int console_lines = 0;                          ///< log lines written with log_console()
    char last_console_line[log::kMaxLineLength] = {}; ///< the latest of them
    int vram_writes_by_core = 0; ///< Vram blocks found modified when freed (must stay 0)
};

const State& state();

/// Forgets recordings and controls (call between tests while the engine is shut down).
void reset();

/// Buttons that read_buttons() reports from now on (mask of nnh::Button bits).
void set_held_buttons(std::uint32_t mask);

/// When true, poll_events() returns false (the system asks the app to quit).
void set_quit_requested(bool quit);

/// When true, init() fails.
void set_init_fails(bool fails);

/// Sets the manual clock returned by ticks_ns() (starts at 0 after reset()).
void set_time_ns(std::int64_t ns);

/// Moves the manual clock forward.
void advance_time(Duration elapsed);

/// Bytes region_alloc() may hand out in a region (defaults: heap 64 MB, linear 32 MB, vram 6 MB).
void set_region_capacity(MemRegion region, std::size_t bytes);

/// The last crash screen requested through crash_show().
struct Crash {
    char top[2048] = {};
    char bottom[2048] = {};
};

/// Runs `fn`; if it reaches crash_show(), the crash is recorded (last_crash()) and control returns
/// here instead of ending the process. @return true if a crash happened.
bool run_expecting_crash(void (*fn)());
const Crash& last_crash();

/// Calls the CPU fault handler core installed with crash_install_hooks(), as if the CPU faulted.
/// Use inside run_expecting_crash().
void simulate_cpu_fault(const platform::CpuFault& fault);

} // namespace nnh::platform_fake
