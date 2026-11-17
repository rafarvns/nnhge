// PC platform: fatal-signal hooks and crash screen in the simulator window (contract
// nnh/platform/crash.hpp).

#include <SDL3/SDL.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "context_pc.hpp"
#include "nnh/platform/crash.hpp"
#include "nnh/platform/video.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace nnh::platform {
namespace {

CpuFaultHandler g_core_handler = nullptr;

const char* signal_name(int sig) {
    switch (sig) {
    case SIGSEGV: return "Segmentation fault (invalid memory access)";
    case SIGFPE: return "Arithmetic exception";
    case SIGILL: return "Illegal instruction";
    case SIGABRT: return "Abort";
    default: return "Fatal signal";
    }
}

void on_signal(int sig) {
    CpuFault fault;
    fault.kind = signal_name(sig);
    if (g_core_handler != nullptr) g_core_handler(fault);
    std::_Exit(EXIT_FAILURE);
}

/// Draws `text` line by line inside a screen rectangle, in the simulator window.
void draw_text_block(Screen screen, const char* text) {
    char line[128];
    int y = 8;
    while (*text != '\0') {
        std::size_t n = 0;
        while (text[n] != '\0' && text[n] != '\n' && n < sizeof line - 1) {
            line[n] = text[n];
            ++n;
        }
        line[n] = '\0';
        debug_text(screen, 6, y, line, colors::white);
        y += debug_line_height();
        text += n;
        if (*text == '\n') ++text;
    }
}

void draw_crash_frame(const char* top, const char* bottom) {
    begin_frame(rgb(120, 16, 16), rgb(40, 8, 8));
    draw_text_block(Screen::Top, top);
    draw_text_block(Screen::Bottom, bottom);
}

void save_screenshot_if_requested() {
    const pc::Context& ctx = pc::context();
    if (ctx.screenshot_path == nullptr) return;
    SDL_Surface* shot = SDL_RenderReadPixels(ctx.renderer, nullptr);
    if (shot != nullptr) {
        SDL_SavePNG(shot, ctx.screenshot_path);
        SDL_DestroySurface(shot);
    }
}

} // namespace

void crash_install_hooks(CpuFaultHandler handler) {
    g_core_handler = handler;
    std::signal(SIGSEGV, on_signal);
    std::signal(SIGFPE, on_signal);
    std::signal(SIGILL, on_signal);
    std::signal(SIGABRT, on_signal);
}

void crash_show(const char* top, const char* bottom) {
    std::fprintf(stderr, "\n==== NNHGE crash ====\n%s\n%s\n", top, bottom);
    std::fflush(stderr);
    pc::Context& ctx = pc::context();
    if (ctx.renderer == nullptr) std::_Exit(EXIT_FAILURE); // no window (not initialized): console only

    if (ctx.exit_after != 0) { // automation (NNH_EXIT_AFTER_FRAMES): one frame, optional screenshot
        draw_crash_frame(top, bottom);
        save_screenshot_if_requested();
        SDL_RenderPresent(ctx.renderer);
        std::_Exit(EXIT_FAILURE);
    }
    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            const bool quit = event.type == SDL_EVENT_QUIT ||
                              (event.type == SDL_EVENT_KEY_DOWN && (event.key.scancode == SDL_SCANCODE_RETURN ||
                                                                    event.key.scancode == SDL_SCANCODE_ESCAPE));
            if (quit) std::_Exit(EXIT_FAILURE);
        }
        draw_crash_frame(top, bottom);
        SDL_RenderPresent(ctx.renderer);
        SDL_Delay(16);
    }
}

bool debugger_attached() {
#if defined(_WIN32)
    return IsDebuggerPresent() != 0;
#else
    return false;
#endif
}

void debug_break() {
#if defined(_WIN32)
    DebugBreak();
#endif
}

} // namespace nnh::platform
