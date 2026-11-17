#pragma once
/// @file nnh/platform_fake.hpp
/// Controls and recordings of the fake platform (nnh::platform_fake), used by unit and conformance
/// tests to drive nnh::core without hardware. Not available to games.

#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/log.hpp"
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

} // namespace nnh::platform_fake
