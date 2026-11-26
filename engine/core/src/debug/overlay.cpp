// Debug overlay on the bottom screen: FPS, frame-time graph, memory per region, 2D draws and tiles, slowest
// profile scopes.
// Recorded into the debug pass (debug_pass.cpp), so it draws with the built-in font on every system.
// Compiled in Debug/Development; set_overlay_visible() has no effect in Release.

#include <cstdio>

#include "debug/dev_tools.hpp"
#include "debug/live_hook.hpp"
#include "debug/overlay_pages.hpp"
#include "gfx/gfx3d_hook.hpp"
#include "nnh/build.hpp"
#include "nnh/debug.hpp"
#include "nnh/input.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/system.hpp"
#include "nnh/profile.hpp"

namespace nnh {
namespace {

constexpr int kHistory = 120;                 // frames in the graph (2 s at 60 Hz)
constexpr std::int64_t kGraphMaxNs = 33'333'333; // bar height scale: 2 frames
constexpr int kGraphHeight = 40;

bool g_visible = false;
Duration g_history[kHistory];
int g_history_next = 0;
int g_history_count = 0;

constexpr std::uint32_t bit(Button b) { return static_cast<std::uint32_t>(b); }

Color bar_color(Duration d) {
    if (d.ns <= 17'000'000) return rgba(80, 220, 120, 220);
    if (d.ns <= 34'000'000) return rgba(240, 200, 60, 220);
    return rgba(240, 70, 60, 220);
}

void draw_frame_stats(int& y, int line) {
    std::int64_t sum = 0;
    Duration worst{};
    for (int i = 0; i < g_history_count; ++i) {
        sum += g_history[i].ns;
        if (g_history[i] > worst) worst = g_history[i];
    }
    const double avg_ms = g_history_count > 0 ? static_cast<double>(sum) / g_history_count / 1e6 : 0.0;
    char text[96];
    std::snprintf(text, sizeof text, "FPS %.1f  %.1f ms (max %.1f)", avg_ms > 0 ? 1000.0 / avg_ms : 0.0, avg_ms,
                  static_cast<double>(worst.ns) / 1e6);
    detail::debug_record_text(Screen::Bottom, 4, y, text, colors::white);
    y += line;
}

void draw_graph(int& y) {
    const int bottom = y + kGraphHeight;
    for (int i = 0; i < g_history_count; ++i) {
        const Duration d = g_history[(g_history_next - g_history_count + i + kHistory) % kHistory];
        int h = static_cast<int>(d.ns * kGraphHeight / kGraphMaxNs);
        if (h > kGraphHeight) h = kGraphHeight;
        if (h < 1) h = 1;
        detail::debug_record_rect(Screen::Bottom, 4 + i * 2, bottom - h, 2, h, bar_color(d));
    }
    y = bottom + 4;
}

void draw_memory(int& y, int line) {
    for (int r = 0; r < kMemRegionCount; ++r) {
        const auto region = static_cast<MemRegion>(r);
        const MemStats stats = mem::region_stats(region);
        const std::size_t capacity = mem::region_capacity(region);
        char text[96];
        if (capacity != 0) {
            std::snprintf(text, sizeof text, "%-6s %6.2f / %6.2f MB", mem::region_name(region),
                          static_cast<double>(stats.bytes) / 1048576.0, static_cast<double>(capacity) / 1048576.0);
        } else {
            std::snprintf(text, sizeof text, "%-6s %6.2f MB", mem::region_name(region),
                          static_cast<double>(stats.bytes) / 1048576.0);
        }
        detail::debug_record_text(Screen::Bottom, 4, y, text, colors::white);
        y += line;
    }
    const gfx::Stats g = gfx::stats();
    char text[96];
    std::snprintf(text, sizeof text, "2D %d/%d draws  %d tex %.2f MB", g.draws_last_frame, g.max_draws, g.textures,
                  static_cast<double>(g.texture_bytes) / 1048576.0);
    detail::debug_record_text(Screen::Bottom, 4, y, text, colors::white);
    y += line;
    if (detail::g_gfx_3d_ops != nullptr && detail::g_gfx_3d_ops->overlay_line(text, sizeof text)) { // spec 026: 3D
        detail::debug_record_text(Screen::Bottom, 4, y, text, colors::white);
        y += line;
    }
    if (g.max_tiles > 0) { // games without tilemaps don't see the line
        std::snprintf(text, sizeof text, "tiles %d/%d  dropped %d", g.tiles_last_frame, g.max_tiles,
                      g.dropped_tiles);
        detail::debug_record_text(Screen::Bottom, 4, y, text,
                                  g.dropped_tiles > 0 ? colors::red : colors::white);
        y += line;
    }
    if (g.max_particles > 0) { // games without particles don't see the line
        std::snprintf(text, sizeof text, "particles %d/%d  dropped %d", g.particles_last_frame, g.max_particles,
                      g.dropped_particles);
        detail::debug_record_text(Screen::Bottom, 4, y, text,
                                  g.dropped_particles > 0 ? colors::red : colors::white);
        y += line;
    }
}

void draw_scopes(int& y, int line) {
    profile::ScopeStats top[4];
    const int n = profile::slowest(top, 4);
    for (int i = 0; i < n; ++i) {
        char text[96];
        std::snprintf(text, sizeof text, "%-14.14s %5.2f ms x%d", top[i].name,
                      static_cast<double>(top[i].total.ns) / 1e6, top[i].calls);
        detail::debug_record_text(Screen::Bottom, 4, y, text, colors::yellow);
        y += line;
    }
}

} // namespace

void debug::set_overlay_visible(bool visible) { g_visible = build::kDevToolsEnabled && visible; }
bool debug::overlay_visible() { return g_visible; }

void detail::overlay_update(Duration frame_delta, std::uint32_t held, std::uint32_t pressed,
                            std::uint32_t platform_dev_requests) {
    if constexpr (!build::kDevToolsEnabled) return;
    g_history[g_history_next] = frame_delta;
    g_history_next = (g_history_next + 1) % kHistory;
    if (g_history_count < kHistory) ++g_history_count;
    // The toggle (SELECT + L, F1) moved to the overlay's pages (spec 030, overlay_pages.cpp): they read the engine
    // controls on paused ticks too.
    (void)held, (void)pressed, (void)platform_dev_requests;
}

void detail::overlay_draw() {
    if (!build::kDevToolsEnabled || !g_visible) return;
    const int line = kFontLineHeight;
    detail::debug_record_rect(Screen::Bottom, 0, 0, screen_width(Screen::Bottom), screen_height(Screen::Bottom),
                        rgba(0, 0, 0, 170));
    if constexpr (build::kDevToolsEnabled) {
        if (detail::overlay_pages_draw()) return; // a live page (spec 030)
    }
    int y = 4;
    draw_frame_stats(y, line);
    draw_graph(y);
    draw_memory(y, line);
    draw_scopes(y, line);
    if constexpr (build::kDevToolsEnabled) { // the live registry's line (spec 030)
        char text[96];
        if (detail::g_live_ops != nullptr && detail::g_live_ops->stats_line(text, sizeof text)) {
            detail::debug_record_text(Screen::Bottom, 4, y, text, colors::white);
        }
    }
}

void detail::overlay_reset() {
    g_history_next = g_history_count = 0;
    if constexpr (build::kDevToolsEnabled) { // a new engine starts with the overlay hidden, on the Stats page (spec 030)
        g_visible = false;
        detail::overlay_pages_reset();
    }
}

} // namespace nnh
