// The live module's ops table, frame end and Stats line (see live_ops.hpp), and its log channel.

#include "live/live_ops.hpp"

#include <cstdio>

#include "debug/live_hook.hpp"
#include "live/live_apply.hpp"
#include "live/live_dump.hpp"
#include "live/live_panel_model.hpp"
#include "live/live_registry.hpp"
#include "live/live_watch.hpp"
#include "nnh/log.hpp"

namespace nnh {

NNH_LOG_CHANNEL(live);

namespace detail::live {

// Filled by the remote part of the module (live_remote.cpp); null until then.
void (*g_remote_op)(std::uint8_t, std::uint8_t, std::uint16_t, std::uint16_t, std::uint32_t, std::uint32_t,
                    std::uint32_t, bool) = nullptr;
void (*g_remote_detached)() = nullptr;
void (*g_follows_frame_end)(Registry&, std::uint32_t, bool) = nullptr;

namespace {

void log_line(void*, [[maybe_unused]] const char* text) { NNH_LOG_INFO(live, "%s", text); }

void panel_input(int page, const DevInput& input, bool) {
    if (g_registry == nullptr) return;
    panel_buttons(*g_registry, page, input);
    if (page == kPageTunables) panel_touch(*g_registry, input);
}
void panel_draw(int page, std::uint32_t frame, bool paused) {
    if (g_registry != nullptr) panel_draw_page(*g_registry, page, frame, paused);
}
void panel_select(int page, const char* name) {
    if (g_registry != nullptr) panel_select_name(*g_registry, page, name);
}
void remote_op(std::uint8_t op, std::uint8_t arg, std::uint16_t count, std::uint16_t seq, std::uint32_t hash,
               std::uint32_t bits, std::uint32_t frame, bool paused) {
    if (g_remote_op != nullptr) g_remote_op(op, arg, count, seq, hash, bits, frame, paused);
}
void remote_detached() {
    if (g_remote_detached != nullptr) g_remote_detached();
}

const LiveOps kOps = {frame_start, frame_end, stats_line, panel_input, panel_draw, panel_select, remote_op,
                      remote_detached};

} // namespace

void install_remote();   // live_remote.cpp
void uninstall_remote();

void install_ops() {
    g_live_ops = &kOps;
    install_remote();
}
void uninstall_ops() {
    g_live_ops = nullptr;
    uninstall_remote();
}

void frame_end(std::uint32_t frame, bool paused) {
    Registry* r = g_registry;
    if (r == nullptr) return;
    if (r->follow_count > 0 && g_follows_frame_end != nullptr) g_follows_frame_end(*r, frame, paused);
    if (r->dump_requested) {
        r->dump_requested = false;
        write_dump(*r, frame, log_line, nullptr);
    }
    if (r->next_sweep != 0 && frame >= r->next_sweep && !paused) {
        sweep_shown(*r, frame);
        r->next_sweep = r->shown_count > 0 ? frame + kSweepEvery : 0;
    }
}

bool stats_line(char* out, int size) {
    const Registry* r = g_registry;
    if (r == nullptr) return false;
    const double kb = static_cast<double>(block_bytes(r->max_tunables, r->max_watches, r->max_shown)) / 1024.0;
    const int n = std::snprintf(out, static_cast<std::size_t>(size), "live %d/%d tun %d/%d wat %d/%d shown %.1f KB",
                                r->tunable_count, r->max_tunables, r->watch_count, r->max_watches, r->shown_count,
                                r->max_shown, kb);
    if (r->refused > 0 && n > 0 && n < size) {
        std::snprintf(out + n, static_cast<std::size_t>(size - n), " refused %lu", static_cast<unsigned long>(r->refused));
    }
    return true;
}

} // namespace detail::live
} // namespace nnh
