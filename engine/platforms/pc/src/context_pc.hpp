#pragma once
// State shared by the PC platform library files. Internal to engine/platforms/pc/.

#include <SDL3/SDL.h>

#include <cstdint>

#include "nnh/gfx.hpp"

namespace nnh::platform::pc {

/// The simulator window shows both screens in a 400x480 logical area.
inline constexpr int kLogicalWidth = 400;
inline constexpr int kLogicalHeight = 480;

struct Context {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::uint64_t frames = 0;               // frames ended since init
    std::uint64_t exit_after = 0;           // NNH_EXIT_AFTER_FRAMES (0 = never)
    const char* screenshot_path = nullptr;  // NNH_SCREENSHOT
    // Developer controls (devtools_pc.cpp)
    std::uint32_t dev_requests = 0;         // platform::DevRequest bits for core
    bool paused = false;
    bool step_requested = false;
    bool slow_motion = false;
    bool screenshot_requested = false;
    bool quit_requested = false;
};

Context& context();

/// Area of a 3DS screen inside the logical window: top at (0,0), bottom centered below it.
inline SDL_FRect screen_rect(Screen screen) {
    if (screen == Screen::Top) return SDL_FRect{0, 0, 400, 240};
    return SDL_FRect{40, 240, 320, 240};
}

inline void set_draw_color(Color c) { SDL_SetRenderDrawColor(context().renderer, c.r, c.g, c.b, c.a); }

/// Starts the 60 Hz pacing clock (called by init).
void reset_frame_pacing();

/// Developer keys (F1, Pause, F10, F11, F12); ignored in Release.
void dev_key_pressed(SDL_Scancode key);
/// Takes a requested F12 screenshot (called before presenting).
void dev_before_present();
/// Blocks while paused, until F10 (one frame), Pause (resume) or quit (called after presenting).
void dev_wait_while_paused();

} // namespace nnh::platform::pc
