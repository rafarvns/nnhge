#pragma once
// State shared by the PC platform library files. Internal to engine/platforms/pc/.

#include <SDL3/SDL.h>

#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/platform/input.hpp"

namespace nnh::platform::pc {

/// The simulator window shows both screens in a 400x480 logical area.
inline constexpr int kLogicalWidth = 400;
inline constexpr int kLogicalHeight = 480;

struct Context {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool gpu_renderer = false;              // renderer runs on an SDL_GPUDevice (see app_pc.cpp)
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
    // Input (input_pc.cpp, gamepad_pc.cpp, touch_pc.cpp)
    SDL_Gamepad* gamepad = nullptr; // the first connected gamepad, or none
    bool mouse_was_down = false;    // left button on the previous sample
    bool mouse_touching = false;    // a touch started on the bottom screen and the button is still down
};

Context& context();

/// Text with SDL's 8x8 debug font, for the crash screen only (debug_text_pc.cpp).
void sdl_text(Screen screen, int x, int y, const char* text, Color color);
int sdl_text_line_height();

/// Sets platform::last_error() to "<what>: <SDL_GetError()>".
void set_error(const char* what);

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

/// Opens/closes the gamepad on SDL_EVENT_GAMEPAD_ADDED/REMOVED (called for every polled event).
void gamepad_event(const SDL_Event& event);
/// Closes the gamepad (called by shutdown).
void gamepad_close();
/// Adds the gamepad's buttons to `buttons` and its sticks to the given ones when they are pushed further.
void gamepad_read(std::uint32_t& buttons, float stick[4]);
/// Left mouse button over the bottom screen → touch, in bottom-screen pixels.
void touch_read(bool& touching, std::int16_t& x, std::int16_t& y);
/// Reads NNH_SIM_INPUT (Debug/Development only; called by init).
void input_override_init();
/// When NNH_SIM_INPUT is set, writes the scripted sample to `out` and returns true (real input is ignored).
bool input_override(InputSample& out);

} // namespace nnh::platform::pc
