// PC simulator: text with SDL's built-in 8x8 font, for the crash screen only (crash_pc.cpp), which must
// work even when the engine is broken. Game and debug text use the engine's built-in font (spec 005).

#include <SDL3/SDL.h>

#include <cstddef>

#include "context_pc.hpp"

namespace nnh::platform::pc {

void sdl_text(Screen screen, int x, int y, const char* text, Color color) {
    SDL_Renderer* renderer = context().renderer;
    draw_on(screen);
    const SDL_FRect area{0, 0, 0, 0};
    set_draw_color(color);

    // SDL_RenderDebugText does not handle '\n': draw line by line.
    const float line = static_cast<float>(sdl_text_line_height());
    float pen_y = area.y + static_cast<float>(y);
    char buffer[256];
    while (*text != '\0') {
        std::size_t n = 0;
        while (text[n] != '\0' && text[n] != '\n' && n < sizeof buffer - 1) {
            buffer[n] = text[n];
            ++n;
        }
        buffer[n] = '\0';
        SDL_RenderDebugText(renderer, area.x + static_cast<float>(x), pen_y, buffer);
        text += n;
        if (*text == '\n') {
            ++text;
            pen_y += line;
        }
    }
}

int sdl_text_line_height() { return SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2; }

} // namespace nnh::platform::pc
