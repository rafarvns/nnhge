// PC simulator: the left mouse button on the bottom screen acts as the 3DS touch screen.
// A touch starts only when the button goes down over the bottom screen; it then follows the mouse
// (clamped to the screen's edges, even outside the window) until the button is released.

#include <SDL3/SDL.h>

#include "context_pc.hpp"

namespace nnh::platform {
namespace {

std::int16_t clamp_to(float v, int max) {
    if (v < 0) return 0;
    if (v > static_cast<float>(max)) return static_cast<std::int16_t>(max);
    return static_cast<std::int16_t>(v);
}

} // namespace

void pc::touch_read(bool& touching, std::int16_t& x, std::int16_t& y) {
    Context& ctx = context();
    float window_x = 0;
    float window_y = 0;
    // While a button is down SDL captures the mouse (SDL_HINT_MOUSE_AUTO_CAPTURE, on by default), so a
    // drag that leaves the window keeps reporting positions.
    const bool down = (SDL_GetMouseState(&window_x, &window_y) & SDL_BUTTON_LMASK) != 0;

    float logical_x = 0;
    float logical_y = 0;
    SDL_RenderCoordinatesFromWindow(ctx.renderer, window_x, window_y, &logical_x, &logical_y);
    const SDL_FRect bottom = screen_rect(Screen::Bottom);
    const float local_x = logical_x - bottom.x;
    const float local_y = logical_y - bottom.y;

    const bool inside = local_x >= 0 && local_y >= 0 && local_x < bottom.w && local_y < bottom.h;
    if (!down) {
        ctx.mouse_touching = false;
    } else if (!ctx.mouse_was_down && inside) {
        ctx.mouse_touching = true;
    }
    ctx.mouse_was_down = down;

    touching = ctx.mouse_touching;
    if (touching) {
        x = clamp_to(local_x, static_cast<int>(bottom.w) - 1);
        y = clamp_to(local_y, static_cast<int>(bottom.h) - 1);
    }
}

} // namespace nnh::platform
