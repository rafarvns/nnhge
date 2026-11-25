// PC simulator: the window view (spec 006) — where the two screen targets go in the window, the window
// keys (F2 layout, F3/F4 scale, F5 fit, F9 or Alt+Enter fullscreen) and the title notice. The layout
// math is pure (sim_layout.cpp); this file applies it with SDL.

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "context_pc.hpp"
#include "sim_layout.hpp"
#include "sim_settings.hpp"

namespace nnh::platform {
namespace {

constexpr int kScreenWidth[2] = {400, 320};
constexpr int kScreenHeight = 240;

void copy_screen(SDL_Renderer* renderer, SDL_Texture* screen, int width, const pc::Placement& p) {
    if (!p.visible || screen == nullptr) return;
    const SDL_FRect dest{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(width * p.scale),
                         static_cast<float>(kScreenHeight * p.scale)};
    SDL_RenderTexture(renderer, screen, nullptr, &dest);
}

constexpr std::uint64_t kNoticeMs = 2000;

/// The scale the window shows now (the fit scale while fitting or in fullscreen).
int effective_scale() {
    const pc::LayoutResult layout = pc::current_layout();
    return layout.top.visible ? layout.top.scale : layout.bottom.scale;
}

/// Shows the view in the window title for a moment: "My Game — Side by side 3x".
void show_notice() {
    pc::Context& ctx = pc::context();
    char text[192];
    const char* layout = pc::layout_title(ctx.layout);
    if (ctx.fullscreen) {
        std::snprintf(text, sizeof text, "%s — %s, fullscreen %dx", ctx.title, layout, effective_scale());
    } else if (ctx.fit) {
        std::snprintf(text, sizeof text, "%s — %s, fit %dx", ctx.title, layout, effective_scale());
    } else {
        std::snprintf(text, sizeof text, "%s — %s %dx", ctx.title, layout, ctx.scale);
    }
    SDL_SetWindowTitle(ctx.window, text);
    ctx.notice_until_ms = SDL_GetTicks() + kNoticeMs;
}

/// The 3D slider and the eye view in the title for a moment: "My Game — 3D slider 0.50, right eye" (spec 026).
void show_stereo_notice() {
    pc::Context& ctx = pc::context();
    static constexpr const char* kViews[3] = {"left eye", "right eye", "anaglyph (red/cyan)"};
    const int h = static_cast<int>(ctx.slider * 100.0f + 0.5f);
    char text[192];
    std::snprintf(text, sizeof text, "%s — 3D slider %d.%02d, %s", ctx.title, h / 100, h % 100,
                  kViews[static_cast<int>(ctx.eye_view)]);
    SDL_SetWindowTitle(ctx.window, text);
    ctx.notice_until_ms = SDL_GetTicks() + kNoticeMs;
}

/// F6/F7: the slider −/+ 0.1 (Shift: 0.01); F8: the eye view. @return true if `key` was one of them.
bool stereo_key(SDL_Scancode key, SDL_Keymod mods) {
    pc::Context& ctx = pc::context();
    const int step = (mods & SDL_KMOD_SHIFT) != 0 ? 1 : 10;
    const int h = static_cast<int>(ctx.slider * 100.0f + 0.5f);
    if (key == SDL_SCANCODE_F6 || key == SDL_SCANCODE_F7) {
        ctx.slider = static_cast<float>(pc::step_slider(h, key == SDL_SCANCODE_F7 ? step : -step)) / 100.0f;
    } else if (key == SDL_SCANCODE_F8) {
        ctx.eye_view = pc::next_eye(ctx.eye_view);
    } else {
        return false;
    }
    show_stereo_notice();
    return true;
}

void set_fullscreen(bool on) {
    pc::Context& ctx = pc::context();
    if (ctx.fullscreen == on) return;
    if (!SDL_SetWindowFullscreen(ctx.window, on)) return; // keep the window as it is
    ctx.fullscreen = on;
    if (!on) pc::view_resize_window();
}

} // namespace

void pc::view_resize_window() {
    Context& ctx = context();
    if (ctx.fullscreen || ctx.fit || ctx.window == nullptr) return;
    // The layout is in render pixels; the window size is in window units (they differ on high-DPI).
    const SimSize size = natural_size(ctx.layout, ctx.scale);
    const float density = SDL_GetWindowPixelDensity(ctx.window) > 0 ? SDL_GetWindowPixelDensity(ctx.window) : 1;
    SDL_SetWindowSize(ctx.window, static_cast<int>(static_cast<float>(size.w) / density + 0.5f),
                      static_cast<int>(static_cast<float>(size.h) / density + 0.5f));
}

void pc::view_key_pressed(SDL_Scancode key, SDL_Keymod mods) {
    Context& ctx = context();
    if (stereo_key(key, mods)) return;
    switch (key) {
    case SDL_SCANCODE_F2:
        ctx.layout = next_layout(ctx.layout);
        view_resize_window();
        break;
    case SDL_SCANCODE_F3:
    case SDL_SCANCODE_F4: {
        // Stepping the scale leaves fit mode, starting from the scale it was showing.
        const int from = ctx.fit && !ctx.fullscreen ? effective_scale() : ctx.scale;
        const int to = from + (key == SDL_SCANCODE_F4 ? 1 : -1);
        ctx.scale = to < 1 ? 1 : (to > kMaxSimScale ? kMaxSimScale : to);
        ctx.fit = false;
        view_resize_window();
        break;
    }
    case SDL_SCANCODE_F5:
        ctx.fit = !ctx.fit;
        view_resize_window();
        break;
    case SDL_SCANCODE_F9: set_fullscreen(!ctx.fullscreen); break;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        if ((mods & SDL_KMOD_ALT) == 0) return;
        set_fullscreen(!ctx.fullscreen);
        break;
    default: return;
    }
    show_notice();
}

void pc::view_update() {
    Context& ctx = context();
    if (ctx.notice_until_ms != 0 && SDL_GetTicks() >= ctx.notice_until_ms) {
        SDL_SetWindowTitle(ctx.window, ctx.title);
        ctx.notice_until_ms = 0;
    }
}

namespace {

/// Scripted runs (screenshots, CI, headless) must not depend on, or change, the developer's settings.
bool scripted_run() {
    const pc::Context& ctx = pc::context();
    const char* driver = SDL_GetCurrentVideoDriver();
    return ctx.exit_after != 0 || ctx.screenshot_path != nullptr ||
           (driver != nullptr && std::strcmp(driver, "offscreen") == 0) || std::getenv("NNH_SIM_LAYOUT") != nullptr ||
           std::getenv("NNH_SIM_SCALE") != nullptr || std::getenv("NNH_SIM_SLIDER") != nullptr ||
           std::getenv("NNH_SIM_EYE") != nullptr;
}

/// NNH_SIM_SLIDER=0..1, NNH_SIM_EYE=left|right|anaglyph (spec 026); a script run (NNH_REMOTE) shows the left eye.
void apply_stereo_environment() {
    pc::Context& ctx = pc::context();
    int h = 0;
    if (pc::parse_slider(std::getenv("NNH_SIM_SLIDER"), h)) ctx.slider = static_cast<float>(h) / 100.0f;
    pc::parse_eye(std::getenv("NNH_SIM_EYE"), ctx.eye_view);
    if (std::getenv("NNH_REMOTE") != nullptr) ctx.eye_view = pc::SimEye::Left;
}

/// NNH_SIM_LAYOUT=stacked|side|large|top|bottom, NNH_SIM_SCALE=1..6|fit.
void apply_environment() {
    pc::Context& ctx = pc::context();
    apply_stereo_environment();
    pc::parse_layout(std::getenv("NNH_SIM_LAYOUT"), ctx.layout);
    if (const char* scale = std::getenv("NNH_SIM_SCALE")) {
        if (std::strcmp(scale, "fit") == 0) {
            ctx.fit = true;
        } else {
            const int value = std::atoi(scale);
            if (value >= 1 && value <= pc::kMaxSimScale) ctx.scale = value;
        }
    }
}

void load_settings_file() {
    pc::Context& ctx = pc::context();
    SDL_IOStream* io = SDL_IOFromFile(ctx.settings_path, "rb");
    if (io == nullptr) return; // first run: defaults
    char text[pc::kMaxSettingsBytes];
    const Sint64 size = SDL_GetIOSize(io);
    const std::size_t read = size > 0 && size <= static_cast<Sint64>(sizeof text) ? SDL_ReadIO(io, text, sizeof text) : 0;
    SDL_CloseIO(io);
    pc::SimSettings s;
    s.scale = ctx.scale; // AppConfig's scale unless the file says otherwise
    if (read == 0 || !pc::parse_settings(text, read, s)) return;
    ctx.layout = s.layout;
    ctx.scale = s.scale;
    ctx.fit = s.fit;
    ctx.fullscreen = s.fullscreen; // applied after the window exists
    ctx.slider = static_cast<float>(s.slider) / 100.0f;
    ctx.eye_view = s.eye;
    ctx.has_window_rect = s.has_window;
    ctx.window_x = s.x;
    ctx.window_y = s.y;
    ctx.window_w = s.w;
    ctx.window_h = s.h;
}

/// True if the rectangle overlaps a connected display by at least 64x64 pixels.
bool on_some_display(int x, int y, int w, int h) {
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    bool found = false;
    for (int i = 0; displays != nullptr && i < count && !found; ++i) {
        SDL_Rect bounds;
        if (!SDL_GetDisplayBounds(displays[i], &bounds)) continue;
        const int overlap_w = SDL_min(x + w, bounds.x + bounds.w) - SDL_max(x, bounds.x);
        const int overlap_h = SDL_min(y + h, bounds.y + bounds.h) - SDL_max(y, bounds.y);
        found = overlap_w >= 64 && overlap_h >= 64;
    }
    SDL_free(displays);
    return found;
}

} // namespace

void pc::view_load(const char* data_folder) {
    Context& ctx = context();
    ctx.persist_view = !scripted_run();
    if (!ctx.persist_view) {
        apply_environment();
        return;
    }
    // Never the game's data folder (saves): a separate per-user folder for the simulator.
    char* folder = SDL_GetPrefPath("NNHGE simulator", data_folder != nullptr ? data_folder : "game");
    if (folder == nullptr) {
        ctx.persist_view = false;
        return;
    }
    std::snprintf(ctx.settings_path, sizeof ctx.settings_path, "%swindow.ini", folder);
    SDL_free(folder);
    load_settings_file();
    if (std::getenv("NNH_REMOTE") != nullptr) ctx.eye_view = SimEye::Left; // driven by nnh run: the left eye
}

pc::SimSize pc::view_initial_window_size() {
    const Context& ctx = context();
    if (ctx.has_window_rect && (ctx.fit || ctx.fullscreen)) return SimSize{ctx.window_w, ctx.window_h};
    return natural_size(ctx.layout, ctx.scale);
}

void pc::view_after_create() {
    Context& ctx = context();
    if (ctx.has_window_rect && on_some_display(ctx.window_x, ctx.window_y, ctx.window_w, ctx.window_h)) {
        SDL_SetWindowPosition(ctx.window, ctx.window_x, ctx.window_y);
    }
    if (ctx.fullscreen) {
        ctx.fullscreen = false;
        set_fullscreen(true);
    } else {
        view_resize_window(); // high-DPI: the natural size is in pixels, the window in window units
    }
}

void pc::view_save() {
    Context& ctx = context();
    if (!ctx.persist_view || ctx.window == nullptr) return;
    if (!ctx.fullscreen) { // in fullscreen keep the last windowed place
        SDL_GetWindowPosition(ctx.window, &ctx.window_x, &ctx.window_y);
        SDL_GetWindowSize(ctx.window, &ctx.window_w, &ctx.window_h);
        ctx.has_window_rect = true;
    }
    SimSettings s;
    s.layout = ctx.layout;
    s.scale = ctx.scale;
    s.fit = ctx.fit;
    s.fullscreen = ctx.fullscreen;
    s.slider = static_cast<int>(ctx.slider * 100.0f + 0.5f);
    s.eye = ctx.eye_view;
    s.has_window = ctx.has_window_rect && ctx.window_w >= 100 && ctx.window_h >= 100;
    s.x = ctx.window_x;
    s.y = ctx.window_y;
    s.w = ctx.window_w;
    s.h = ctx.window_h;
    char text[kMaxSettingsBytes];
    const int length = format_settings(s, text, sizeof text);
    if (length <= 0) return;
    if (SDL_IOStream* io = SDL_IOFromFile(ctx.settings_path, "wb")) {
        SDL_WriteIO(io, text, static_cast<std::size_t>(length));
        SDL_CloseIO(io);
    }
}

pc::LayoutResult pc::current_layout() {
    Context& ctx = context();
    int width = 0;
    int height = 0;
    SDL_GetCurrentRenderOutputSize(ctx.renderer, &width, &height);
    const int scale = ctx.fit || ctx.fullscreen ? fit_scale(ctx.layout, width, height) : ctx.scale;
    return compute_layout(ctx.layout, scale, width, height);
}

void pc::compose_screens() {
    Context& ctx = context();
    SDL_SetRenderTarget(ctx.renderer, nullptr);
    const LayoutResult layout = current_layout();
    SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_NONE);
    set_draw_color(rgb(12, 12, 12)); // around the screens
    SDL_RenderClear(ctx.renderer);
    if (ctx.compose_top != nullptr && layout.top.visible) { // a 3D game: the eye view (eyes_pc.cpp, spec 026)
        const SDL_FRect dest{static_cast<float>(layout.top.x), static_cast<float>(layout.top.y),
                             static_cast<float>(kScreenWidth[0] * layout.top.scale),
                             static_cast<float>(kScreenHeight * layout.top.scale)};
        ctx.compose_top(dest);
    } else {
        copy_screen(ctx.renderer, ctx.screens[0], kScreenWidth[0], layout.top);
    }
    copy_screen(ctx.renderer, ctx.screens[1], kScreenWidth[1], layout.bottom);
    SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
}

} // namespace nnh::platform
