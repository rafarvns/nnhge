// Advancing one slot (spec 015 FR-6, FR-7, FR-13, FR-16): delay, start, loops (restart or yoyo) with the
// leftover carried into the next loop, finish. Integer ticks only: nothing drifts.

#include "tween/tween_internal.hpp"

namespace nnh::detail {

void tween_advance_slot(TweenBlock& b, int index, std::int32_t ticks) {
    SlotHot& s = b.hot[index];
    if ((s.flags & slot_flag::kPaused) != 0) return;
    if ((s.flags & slot_flag::kStarted) == 0) {
        if (ticks < s.delay) {
            s.delay -= ticks;
            return;
        }
        ticks -= s.delay;
        s.delay = 0;
        tween_begin(s);
    }
    s.elapsed += ticks; // both ≤ 1 hour of ticks: no overflow
    const std::uint32_t generation = s.generation;
    while (s.elapsed >= s.duration) {
        const bool backwards = (s.flags & slot_flag::kBackwards) != 0;
        if (s.loops_left != kForever && --s.loops_left == 0) {
            s.elapsed = s.duration;
            tween_finish(b, index, backwards);
            return;
        }
        s.elapsed -= s.duration;
        if ((s.flags & slot_flag::kYoyo) != 0) s.flags ^= slot_flag::kBackwards;
        tween_loop_callback(b, index);
        // The callback may have killed, completed or paused this tween: stop here, the rest of the time is
        // kept.
        if (s.generation != generation || (s.flags & slot_flag::kPaused) != 0) break;
    }
    if (s.generation == generation) tween_write_current(s);
}

} // namespace nnh::detail
