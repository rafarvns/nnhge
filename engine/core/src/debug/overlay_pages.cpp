// The debug overlay's pages (overlay_pages.hpp). Debug/Development only.

#include "debug/overlay_pages.hpp"

#include "app/dev_pause.hpp"
#include "debug/dev_controls.hpp"
#include "debug/dev_tools.hpp"
#include "debug/live_hook.hpp"
#include "input/input_block.hpp"
#include "nnh/app.hpp"
#include "nnh/debug.hpp"
#include "nnh/input.hpp"
#include "nnh/platform/system.hpp"
#include "nnh/profile.hpp"

namespace nnh::detail {
namespace {

int g_page = kPageStats;
bool g_blocking = false;

constexpr std::uint32_t bit(Button b) { return static_cast<std::uint32_t>(b); }

bool chord(const DevInput& in, Button b) {
    return (in.held & bit(Button::Select)) != 0 && (in.pressed & bit(b)) != 0;
}

/// The panel has the controls on a live page while the overlay is shown (D38's blocker: the game reads idle, held
/// controls stay filtered until released).
void update_blocker() {
    const bool want = debug::overlay_visible() && g_page != kPageStats;
    if (want == g_blocking) return;
    g_blocking = want;
    input_block(InputBlocker::DevPanel, want);
}

} // namespace

void overlay_pages_input(const DevInput& in, std::uint32_t requests, bool paused) {
    if (chord(in, Button::L) || (requests & platform::kDevToggleOverlay) != 0) {
        debug::set_overlay_visible(!debug::overlay_visible());
    }
    if (debug::overlay_visible() && chord(in, Button::R)) g_page = (g_page + 1) % 3;
    update_blocker();
    if (!debug::overlay_visible() || g_page == kPageStats) return;
    if (chord(in, Button::Y)) dev_pause_state().toggle();
    if (chord(in, Button::X)) dev_pause_state().step(1);
    if (g_live_ops != nullptr) g_live_ops->panel_input(g_page, in, paused);
}

int overlay_page() { return g_page; }

void overlay_show_page(int page) {
    g_page = page < 0 || page > 2 ? kPageStats : page;
    debug::set_overlay_visible(true);
    update_blocker();
}

bool overlay_pages_draw() {
    if (g_page == kPageStats) return false;
#if defined(NNH_FLAVOR_DEVELOPMENT)
    NNH_PROFILE_SCOPE("live page"); // the bench's page cost (CPU: model, layout, text, recording)
#endif
    const auto frame = static_cast<std::uint32_t>(frame_count());
    const bool paused = dev_pause_state().paused();
    if (g_live_ops != nullptr) {
        g_live_ops->panel_draw(g_page, frame, paused);
    } else {
        debug_record_text(Screen::Bottom, 4, 2, g_page == 1 ? "Tunables" : "Watches", colors::white);
        debug_record_text(Screen::Bottom, 10, 16, "no live values in this game", colors::gray);
    }
    return true;
}

void overlay_pages_reset() {
    g_page = kPageStats;
    if (g_blocking) input_block(InputBlocker::DevPanel, false);
    g_blocking = false;
}

} // namespace nnh::detail
