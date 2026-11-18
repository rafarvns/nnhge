// PC simulator: the left mouse button on the bottom screen acts as the 3DS touch screen, wherever the
// window layout draws it (spec 006).
// A touch starts only when the button goes down over the bottom screen; it then follows the mouse
// (clamped to the screen's edges, even outside the window) until the button is released.

#include <SDL3/SDL.h>

#include "context_pc.hpp"

namespace nnh::platform {

void pc::touch_read(bool& touching, std::int16_t& x, std::int16_t& y) {
    Context& ctx = context();
    float window_x = 0;
    float window_y = 0;
    // While a button is down SDL captures the mouse (SDL_HINT_MOUSE_AUTO_CAPTURE, on by default), so a
    // drag that leaves the window keeps reporting positions.
    const bool down = (SDL_GetMouseState(&window_x, &window_y) & SDL_BUTTON_LMASK) != 0;

    // Window coordinates → render-output pixels (high-DPI aware), then through the current layout.
    float pixel_x = 0;
    float pixel_y = 0;
    SDL_RenderCoordinatesFromWindow(ctx.renderer, window_x, window_y, &pixel_x, &pixel_y);
    const LayoutResult layout = current_layout();
    float local_x = 0;
    float local_y = 0;
    const bool inside = window_to_bottom(layout, pixel_x, pixel_y, false, local_x, local_y);
    if (!down || !layout.bottom.visible) {
        ctx.mouse_touching = false;
    } else if (!ctx.mouse_was_down && inside) {
        ctx.mouse_touching = true;
    }
    ctx.mouse_was_down = down;

    touching = ctx.mouse_touching;
    if (touching && window_to_bottom(layout, pixel_x, pixel_y, true, local_x, local_y)) {
        x = static_cast<std::int16_t>(local_x);
        y = static_cast<std::int16_t>(local_y);
    }
}

} // namespace nnh::platform
