// Completion (spec 015 FR-11, FR-12): the final value, then the slot ends (its handles go stale, so
// completing it again from its own callback does nothing), then on_finish and the game's event. Callbacks
// that complete tweens whose callbacks complete tweens... stop calling back at kMaxCallbackDepth, with one
// logged error.

#include "nnh/events.hpp"
#include "tween/tween_internal.hpp"

namespace nnh::detail {
namespace {

bool callbacks_allowed(TweenBlock& b) {
    if (b.callback_depth < kMaxCallbackDepth) return true;
    if (!b.depth_logged) {
        b.depth_logged = true;
        NNH_LOG_ERROR(
            tween, "TweenSet: callback depth limit (%d) reached: completions skip their callbacks and events",
            kMaxCallbackDepth);
    }
    return false;
}

} // namespace

void tween_finish(TweenBlock& b, int index, bool backwards_end) {
    SlotHot& s = b.hot[index];
    tween_write(s, backwards_end ? s.from : s.to);
    tween_kill_slot(b, index); // the block is busy: the slot (and its cold data) stays until the sweep
    tween_run_completion(b, index);
}

void tween_run_completion(TweenBlock& b, int index) {
    const SlotCold& c = b.cold[index];
    if ((!c.on_finish && !c.publish) || !callbacks_allowed(b)) return;
    if (c.on_finish) {
        ++b.callback_depth;
        c.on_finish();
        --b.callback_depth;
    }
    if (c.publish) events_publish(c.publish.type, c.publish.bytes, c.publish.size);
}

void tween_loop_callback(TweenBlock& b, int index) {
    const SlotCold& c = b.cold[index];
    if (!c.on_loop || !callbacks_allowed(b)) return;
    ++b.callback_depth;
    c.on_loop();
    --b.callback_depth;
}

} // namespace nnh::detail
