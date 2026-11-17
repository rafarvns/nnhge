// PC simulator backend (SDL3). Shows both 3DS screens in one window:
// the top screen (400x240) above the bottom screen (320x240, centered).
//
// Developer environment variables (simulator only):
//   NNH_EXIT_AFTER_FRAMES=<n>   quit after n frames (smoke tests, CI)
//   NNH_SCREENSHOT=<file.png>   save the last frame before quitting (needs NNH_EXIT_AFTER_FRAMES)

#include <SDL3/SDL.h>
// The game owns main(); SDL must not replace it.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <cstdlib>

#include "nnh/input.hpp"
#include "platform/platform.hpp"

namespace nnh::platform {
namespace {

constexpr int kLogicalWidth = 400;
constexpr int kLogicalHeight = 480;
constexpr Uint64 kFrameNs = 1'000'000'000ull / 60;

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
Uint64 g_next_frame_ns = 0;
std::uint64_t g_frames = 0;
std::uint64_t g_exit_after = 0;
const char* g_screenshot_path = nullptr;

SDL_FRect screen_rect(Screen screen) {
    if (screen == Screen::Top) return SDL_FRect{0, 0, 400, 240};
    return SDL_FRect{40, 240, 320, 240};
}

void set_color(Color c) { SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a); }

struct KeyBinding {
    SDL_Scancode key;
    Button button;
};

// Keep in sync with docs/api/input.md.
constexpr KeyBinding kKeyBindings[] = {
    {SDL_SCANCODE_X, Button::A},          {SDL_SCANCODE_Z, Button::B},
    {SDL_SCANCODE_S, Button::X},          {SDL_SCANCODE_A, Button::Y},
    {SDL_SCANCODE_Q, Button::L},          {SDL_SCANCODE_W, Button::R},
    {SDL_SCANCODE_1, Button::ZL},         {SDL_SCANCODE_2, Button::ZR},
    {SDL_SCANCODE_RETURN, Button::Start}, {SDL_SCANCODE_BACKSPACE, Button::Select},
    {SDL_SCANCODE_UP, Button::Up},        {SDL_SCANCODE_DOWN, Button::Down},
    {SDL_SCANCODE_LEFT, Button::Left},    {SDL_SCANCODE_RIGHT, Button::Right},
};

void save_screenshot() {
    SDL_Surface* shot = SDL_RenderReadPixels(g_renderer, nullptr);
    if (shot == nullptr) {
        SDL_Log("NNHGE: screenshot failed: %s", SDL_GetError());
        return;
    }
    if (!SDL_SavePNG(shot, g_screenshot_path)) SDL_Log("NNHGE: screenshot failed: %s", SDL_GetError());
    SDL_DestroySurface(shot);
}

} // namespace

bool init(const AppConfig& config) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("NNHGE: SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    const int scale = config.pc_window_scale > 0 ? config.pc_window_scale : 1;
    if (!SDL_CreateWindowAndRenderer(config.title, kLogicalWidth * scale, kLogicalHeight * scale,
                                     SDL_WINDOW_RESIZABLE, &g_window, &g_renderer)) {
        SDL_Log("NNHGE: window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }
    SDL_SetRenderLogicalPresentation(g_renderer, kLogicalWidth, kLogicalHeight,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderVSync(g_renderer, 1);

    if (const char* n = std::getenv("NNH_EXIT_AFTER_FRAMES")) g_exit_after = std::strtoull(n, nullptr, 10);
    g_screenshot_path = std::getenv("NNH_SCREENSHOT");
    g_frames = 0;
    g_next_frame_ns = SDL_GetTicksNS();
    return true;
}

void shutdown() {
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    g_renderer = nullptr;
    g_window = nullptr;
    SDL_Quit();
}

bool poll_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return false;
    }
    return g_exit_after == 0 || g_frames < g_exit_after;
}

std::uint32_t read_buttons() {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    std::uint32_t mask = 0;
    for (const KeyBinding& binding : kKeyBindings) {
        if (keys[binding.key]) mask |= static_cast<std::uint32_t>(binding.button);
    }
    return mask;
}

void begin_frame(Color top_clear, Color bottom_clear) {
    SDL_SetRenderClipRect(g_renderer, nullptr);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);
    set_color(rgb(12, 12, 12)); // area outside the screens
    SDL_RenderClear(g_renderer);
    const SDL_FRect top = screen_rect(Screen::Top);
    const SDL_FRect bottom = screen_rect(Screen::Bottom);
    set_color(top_clear);
    SDL_RenderFillRect(g_renderer, &top);
    set_color(bottom_clear);
    SDL_RenderFillRect(g_renderer, &bottom);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
}

void end_frame() {
    SDL_SetRenderClipRect(g_renderer, nullptr);
    ++g_frames;
    if (g_screenshot_path != nullptr && g_exit_after != 0 && g_frames == g_exit_after) save_screenshot();
    SDL_RenderPresent(g_renderer);

    // Hold the 3DS rate (60 Hz) even on faster monitors.
    g_next_frame_ns += kFrameNs;
    const Uint64 now = SDL_GetTicksNS();
    if (now < g_next_frame_ns) {
        SDL_DelayPrecise(g_next_frame_ns - now);
    } else if (now - g_next_frame_ns > kFrameNs * 4) {
        g_next_frame_ns = now; // fell far behind (debugger, window drag): don't try to catch up
    }
}

void debug_text(Screen screen, int x, int y, const char* text, Color color) {
    const SDL_FRect area = screen_rect(screen);
    const SDL_Rect clip{static_cast<int>(area.x), static_cast<int>(area.y), static_cast<int>(area.w),
                        static_cast<int>(area.h)};
    SDL_SetRenderClipRect(g_renderer, &clip);
    set_color(color);
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
        SDL_RenderDebugText(g_renderer, area.x + static_cast<float>(x), pen_y, buffer);
        text += n;
        if (*text == '\n') {
            ++text;
            pen_y += line;
        }
    }
}

int debug_line_height() { return SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2; }

} // namespace nnh::platform
