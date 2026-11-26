#pragma once
// The debug overlay's pages (spec 030 FR-7, FR-10, Q4): Stats → Tunables → Watches (SELECT + R while shown), the
// DevPanel input blocker while a live page has the controls, the pause chords (SELECT + Y pause/resume, SELECT + X step)
// and live::show_panel(). The overlay's toggle (SELECT + L, the simulator's F1) is taken here too, from the engine
// controls of the tick, so it works on paused ticks. Debug/Development only. Tests: tests/debug/test_overlay_pages.cpp.

#include <cstdint>

namespace nnh::detail {

struct DevInput;

inline constexpr int kPageStats = 0;

/// The engine controls of this tick (a game frame's start, or a paused tick) and the platform's requests.
void overlay_pages_input(const DevInput& input, std::uint32_t requests, bool paused);

/// The page shown (kPageStats, 1 Tunables, 2 Watches).
int overlay_page();

/// Shows the overlay on `page` (live::show_panel), the panel taking the controls on a live page.
void overlay_show_page(int page);

/// Draws a live page (or its "no live values" note) into the debug pass. @return false on the Stats page.
bool overlay_pages_draw();

/// The Stats page and no blocker (nnh::init()).
void overlay_pages_reset();

} // namespace nnh::detail
