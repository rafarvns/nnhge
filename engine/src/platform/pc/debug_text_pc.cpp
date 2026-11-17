// PC simulator: debug text with SDL's built-in 8x8 font.

#include <SDL3/SDL.h>

#include <cstddef>

#include "platform/pc/context_pc.hpp"
#include "platform/platform.hpp"

namespace nnh::platform {

void debug_text(Screen screen, int x, int y, const char* text, Color color) {
    SDL_Renderer* renderer = pc::context().renderer;
    const SDL_FRect area = pc::screen_rect(screen);
    const SDL_Rect clip{static_cast<int>(area.x), static_cast<int>(area.y), static_cast<int>(area.w),
                        static_cast<int>(area.h)};
    SDL_SetRenderClipRect(renderer, &clip);
    pc::set_draw_color(color);

    // SDL_RenderDebugText does not handle '\n': draw line by line.
    const float line = static_cast<float>(debug_line_height());
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

int debug_line_height() { return SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2; }

} // namespace nnh::platform
