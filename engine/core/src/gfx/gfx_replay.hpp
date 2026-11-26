#pragma once
// The held frame (spec 030 D52): in Debug/Development builds the last game frame's 2D, tile and particle lists stay
// recorded after its submit, so paused ticks can submit them again under a live overlay; they are cleared when the next
// game frame's recording begins. The debug text the game recorded is replayed the same way (debug_held.cpp).
// Tests: tests/gfx/test_gfx_replay.cpp.

namespace nnh::detail {

/// The frame's lists were submitted and are held (set by gfx_submit()'s Development branch).
extern bool g_gfx_held;

/// Clears the held lists (the last statement of nnh::frame() before the game records; Development only).
void gfx_begin_recording();

/// Submits the held lists again (a paused tick). Textures released since are skipped. @return false when nothing is
/// held (a held start) or the replay was dropped because scene changes were applied at the start of the held frame.
bool gfx_replay(bool scenes_changed);

/// Debug text: entries recorded through debug_record_text/rect since the last flush (debug.cpp's Development branch).
extern int g_debug_recorded;
/// How many of the last game frame's entries were the game's (before the overlay): replayed on paused ticks.
extern int g_debug_game_mark;
/// Copies the held frame's game entries into this tick's list (before the overlay draws).
void debug_replay_game_entries();

} // namespace nnh::detail
