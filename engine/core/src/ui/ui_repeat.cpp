// Held-control repeat (ui_repeat.hpp).

#include "ui/ui_repeat.hpp"

namespace nnh::detail {
namespace {

constexpr std::uint8_t kDirections = 0x0F;
constexpr std::uint8_t kPages = 0xC0;

} // namespace

RepeatFired repeat_step(RepeatState& s, std::uint8_t held, std::uint8_t pressed, std::uint8_t given_repeats,
                        int delay, int interval) {
    RepeatFired out;
    if ((pressed & held) != 0) ++s.sequence; // one number per frame: controls pressed together tie
    std::uint8_t timed = 0;
    for (int c = 0; c < 8; ++c) {
        const auto bit = static_cast<std::uint8_t>(1u << c);
        if ((held & bit) == 0) {
            s.frames[c] = 0;
            s.pressed_at[c] = 0;
            continue;
        }
        if ((pressed & bit) != 0) {
            s.frames[c] = 0;
            s.pressed_at[c] = s.sequence;
            out.fresh |= bit;
            continue;
        }
        if (s.pressed_at[c] == 0) continue; // held, but its press wasn't seen
        if (s.frames[c] < 0xFFFF) ++s.frames[c];
        const int f = s.frames[c];
        if (delay == 0) {
            if ((given_repeats & bit) != 0) timed |= bit;
        } else if (f >= delay && (f - delay) % interval == 0) {
            timed |= bit;
        }
    }
    out.repeat = static_cast<std::uint8_t>(timed & (kDirections | kPages));
    // one direction: the newest pressed of those held (ties: the lowest bit)
    int newest = -1;
    for (int c = 0; c < 4; ++c) {
        if (s.pressed_at[c] != 0 && (newest < 0 || s.pressed_at[c] > s.pressed_at[newest])) newest = c;
    }
    const std::uint8_t keep = newest < 0 ? 0 : static_cast<std::uint8_t>(1u << newest);
    out.fresh = static_cast<std::uint8_t>((out.fresh & ~kDirections) | (out.fresh & keep));
    out.repeat = static_cast<std::uint8_t>((out.repeat & ~kDirections) | (out.repeat & keep));
    return out;
}

} // namespace nnh::detail
