// The held frame's debug text (gfx/gfx_replay.hpp): on a paused tick the game's entries of the last game frame are
// recorded again before the overlay draws. The list a paused tick flushes starts with those same entries, so the mark
// stays right tick after tick. Development only.

#include "debug/debug_list.hpp"
#include "debug/dev_tools.hpp"
#include "gfx/gfx_replay.hpp"

namespace nnh::detail {

int g_debug_recorded = 0;
int g_debug_game_mark = 0;

void debug_replay_game_entries() {
    const DebugList& held = debug_list_last_frame();
    const int n = g_debug_game_mark < held.count() ? g_debug_game_mark : held.count();
    for (int i = 0; i < n; ++i) {
        const DebugText& e = held.at(i);
        if (e.kind == DebugText::Kind::Rect) debug_record_rect(e.screen, e.x, e.y, e.w, e.h, e.color);
        else debug_record_text(e.screen, e.x, e.y, held.text(i), e.color);
    }
}

} // namespace nnh::detail
