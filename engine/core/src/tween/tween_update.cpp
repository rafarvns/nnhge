// The set's update loop (spec 015 FR-12, FR-16..FR-18): one pass over the start-order list, kills during the
// pass only mark slots, a sweep frees them afterwards; the speed and the advance remainder in exact integer
// math.

#include "nnh/assert.hpp"
#include "nnh/profile.hpp"
#include "tween/tween_internal.hpp"

namespace nnh {
namespace detail {
namespace {

void unlink_and_free(TweenBlock& b, int index) {
    SlotHot& s = b.hot[index];
    if (s.kind == TweenKind::Sequence) sequence_release(b, index);
    if (s.prev >= 0) {
        b.hot[s.prev].next = s.next;
    } else {
        b.head = s.next;
    }
    if (s.next >= 0) {
        b.hot[s.next].prev = s.prev;
    } else {
        b.tail = s.prev;
    }
    s.state = SlotState::Free;
    s.prev = -1;
    s.next = b.free_head;
    b.free_head = static_cast<std::int16_t>(index);
}

/// One pass: every slot alive when the pass starts, in start order. Slots started during the pass come after
/// the tail seen here, so they begin counting at the next pass (FR-12).
} // namespace

void tween_pass(TweenBlock& b, std::int32_t ticks) {
    NNH_PROFILE_SCOPE("tween update");
    if (ticks <= 0 || b.head < 0) return;
    tween_enter(b);
    const int last = b.tail;
    for (int i = b.head; i >= 0;) {
        if (b.hot[i].state == SlotState::Active) tween_advance_slot(b, i, ticks);
        if (i == last) break;
        i = b.hot[i].next; // still linked: nothing is unlinked while busy
    }
    tween_leave(b);
}

namespace {

bool usable_for_update(const TweenBlock* b) {
    if (b == nullptr) return false;
    if (b->scene != nullptr) {
        NNH_LOG_ERROR(tween, "TweenSet: update()/advance() on a set bound to a scene (its scene updates it)");
        NNH_ASSERT(false, "nnh::TweenSet: a bound set is updated by its scene, not by hand");
        return false;
    }
    if (b->busy == 0) return true;
    NNH_LOG_ERROR(tween, "TweenSet: update()/advance() called from inside its own update (ignored)");
    NNH_ASSERT(false, "nnh::TweenSet: update()/advance() from inside its own update or callback");
    return false;
}

} // namespace

void tween_enter(TweenBlock& b) {
    ++b.busy;
}

void tween_leave(TweenBlock& b) {
    if (--b.busy != 0) return;
    if (b.doomed) {
        // Its set was destroyed from a callback (misuse, logged): nothing touches the block after this.
        mem::free(&b);
        return;
    }
    b.depth_logged = false;
    if (b.dead > 0) tween_sweep(b);
}

void tween_kill_slot(TweenBlock& b, int index) {
    SlotHot& s = b.hot[index];
    if (++s.generation == 0) s.generation = 1; // handles go stale now
    --b.active;
    if (b.busy > 0) {
        s.state = SlotState::Dead;
        ++b.dead;
    } else {
        unlink_and_free(b, index);
    }
}

void tween_sweep(TweenBlock& b) {
    for (int i = b.head; i >= 0;) {
        const int next = b.hot[i].next;
        if (b.hot[i].state == SlotState::Dead) unlink_and_free(b, i);
        i = next;
    }
    b.dead = 0;
}

} // namespace detail

void TweenSet::update() {
    if (detail::usable_for_update(block_)) detail::tween_pass(*block_, block_->speed_ticks);
}

void TweenSet::advance(FrameTime time) {
    if (!time.valid()) {
        NNH_LOG_ERROR(tween, "TweenSet::advance: the time is negative or over 1 hour (ignored)");
        NNH_ASSERT(false, "nnh::TweenSet::advance: invalid time");
        return;
    }
    if (!detail::usable_for_update(block_)) return;
    // ticks × speed / 4096, the remainder kept for the next call: no error accumulates.
    const std::int64_t scaled =
        static_cast<std::int64_t>(time.ticks) * block_->speed_ticks + block_->remainder;
    block_->remainder = static_cast<std::int32_t>(scaled % FrameTime::kTicksPerFrame);
    const std::int64_t ticks = scaled / FrameTime::kTicksPerFrame;
    detail::tween_pass(*block_,
                 static_cast<std::int32_t>(ticks < FrameTime::kMaxTicks ? ticks : FrameTime::kMaxTicks));
}

void TweenSet::set_speed(float speed) {
    if (!(speed >= 0.0f && speed <= 64.0f)) {
        NNH_LOG_ERROR(tween, "TweenSet::set_speed: %g is outside 0..64 (ignored)",
                      static_cast<double>(speed));
        NNH_ASSERT(false, "nnh::TweenSet::set_speed: speed outside 0..64");
        return;
    }
    if (block_ != nullptr) {
        block_->speed_ticks = static_cast<std::int32_t>(speed * FrameTime::kTicksPerFrame + 0.5f);
    }
}

float TweenSet::speed() const {
    if (block_ == nullptr) return 1.0f;
    return static_cast<float>(block_->speed_ticks) / static_cast<float>(FrameTime::kTicksPerFrame);
}

} // namespace nnh
