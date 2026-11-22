// Completion (spec 015 FR-11, FR-12): the final value, then the slot ends (its handles go stale), then
// on_finish and the game's event.

#include "tween/tween_internal.hpp"

namespace nnh::detail {

void tween_finish(TweenBlock& b, int index, bool backwards_end) {
    SlotHot& s = b.hot[index];
    tween_write(s, backwards_end ? s.from : s.to);
    tween_kill_slot(b, index);
}

void tween_loop_callback(TweenBlock&, int) {}

} // namespace nnh::detail
