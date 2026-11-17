#pragma once
// State shared by the PC backend files. Internal to engine/src/platform/pc/.

#include <SDL3/SDL.h>

#include "nnh/gfx.hpp"

namespace nnh::platform::pc {

/// The simulator window shows both screens in a 400x480 logical area.
inline constexpr int kLogicalWidth = 400;
inline constexpr int kLogicalHeight = 480;

struct Context {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
};

Context& context();

/// Area of a 3DS screen inside the logical window: top at (0,0), bottom centered below it.
inline SDL_FRect screen_rect(Screen screen) {
    if (screen == Screen::Top) return SDL_FRect{0, 0, 400, 240};
    return SDL_FRect{40, 240, 320, 240};
}

inline void set_draw_color(Color c) { SDL_SetRenderDrawColor(context().renderer, c.r, c.g, c.b, c.a); }

} // namespace nnh::platform::pc
