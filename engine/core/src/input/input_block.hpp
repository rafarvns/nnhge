#pragma once
// The input block (spec 023 FR-12, decision D38): while a blocker is on (the game's input::set_blocked or a
// transition), the frame's sample is replaced by an idle one before the input module reads it; when the last
// blocker lifts, controls still held stay filtered until released. Pure part: InputBlock (unit tested in
// tests/input/test_input_block.cpp). Linked only by games that block: input.cpp reaches it through
// detail::g_input_filter (input_internal.hpp), set only while blocking or while something stays filtered.

#include <cstdint>

namespace nnh::platform {
struct InputSample;
}

namespace nnh::detail {

/// Who blocks: the game and transitions are independent (one never lifts the other).
enum class InputBlocker : std::uint8_t { Game = 1, Transition = 2 };

struct InputBlock {
    std::uint32_t filtered = 0; ///< buttons held when the block lifted (or pressed during it), until released
    std::uint8_t blockers = 0;  ///< InputBlocker bits
    bool touch = false;         ///< a touch held: reads "not touching" until lifted
    bool circle = false;        ///< the Circle Pad out of its dead zone: reads (0, 0) until back in it once
    bool cstick = false;        ///< the same for the C-Stick

    void set(InputBlocker who, bool on);
    bool blocking() const { return blockers != 0; }
    /// Blocking, or a control still filtered: the filter must keep running.
    bool needed() const { return blockers != 0 || filtered != 0 || touch || circle || cstick; }
    /// Filters this frame's raw sample in place. @return true while blocking (the sample is idle and the input module
    /// must make no edge from it: nothing pressed, released, started or ended).
    bool filter(platform::InputSample& sample);
};

/// Turns one blocker on or off (input::set_blocked for the game; the transition module for transitions).
void input_block(InputBlocker who, bool on);

} // namespace nnh::detail
