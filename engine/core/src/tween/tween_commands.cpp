// Queries and commands through handles, and group commands (spec 015 FR-8, FR-9). Stale handles answer "not
// running" and do nothing.

#include "tween/tween_internal.hpp"

namespace nnh {
namespace detail {
namespace {

/// Whether the last loop of a slot plays backwards (complete(): where a finite yoyo ends).
bool ends_backwards(const SlotHot& s) {
    const bool now = (s.flags & slot_flag::kBackwards) != 0;
    if (s.loops_left == kForever || (s.flags & slot_flag::kYoyo) == 0) return now;
    return ((s.loops_left - 1) % 2 == 1) ? !now : now;
}

template <typename Fn> void for_each_active(TweenBlock* b, Fn fn) {
    if (b == nullptr) return;
    for (int i = b->head; i >= 0;) {
        const int next = b->hot[i].next; // read first: fn may free slot i (outside an update)
        if (b->hot[i].state == SlotState::Active) fn(*b, i);
        i = next;
    }
}

} // namespace
} // namespace detail

using detail::slot_flag::kPaused;

bool TweenSet::running(TweenHandle h) const {
    return detail::tween_find(block_, h) >= 0;
}

bool TweenSet::paused(TweenHandle h) const {
    const int i = detail::tween_find(block_, h);
    return i >= 0 && (block_->hot[i].flags & kPaused) != 0;
}

float TweenSet::progress(TweenHandle h) const {
    const int i = detail::tween_find(block_, h);
    if (i < 0) return 0.0f;
    const detail::SlotHot& s = block_->hot[i];
    if (s.duration <= 0) return 0.0f; // elapsed stays 0 until the tween starts: 0 during the delay
    return static_cast<float>(s.elapsed) / static_cast<float>(s.duration);
}

int TweenSet::loops_left(TweenHandle h) const {
    const int i = detail::tween_find(block_, h);
    return i >= 0 ? block_->hot[i].loops_left : 0;
}

void TweenSet::pause(TweenHandle h) {
    const int i = detail::tween_find(block_, h);
    if (i >= 0) block_->hot[i].flags |= kPaused;
}

void TweenSet::resume(TweenHandle h) {
    const int i = detail::tween_find(block_, h);
    if (i >= 0) block_->hot[i].flags &= static_cast<std::uint8_t>(~kPaused);
}

void TweenSet::kill(TweenHandle h) {
    const int i = detail::tween_find(block_, h);
    if (i >= 0) detail::tween_kill_slot(*block_, i);
}

void TweenSet::complete(TweenHandle h) {
    const int i = detail::tween_find(block_, h);
    if (i < 0) return;
    detail::SlotHot& s = block_->hot[i];
    if ((s.flags & detail::slot_flag::kStarted) == 0) {
        s.delay = 0;
        detail::tween_begin(s);
    }
    detail::tween_enter(*block_); // callbacks may run: kills only mark slots until the outermost leave
    detail::tween_finish(*block_, i, detail::ends_backwards(s));
    detail::tween_leave(*block_);
}

void TweenSet::pause_all() {
    detail::for_each_active(block_, [](detail::TweenBlock& b, int i) { b.hot[i].flags |= kPaused; });
}

void TweenSet::resume_all() {
    detail::for_each_active(
        block_, [](detail::TweenBlock& b, int i) { b.hot[i].flags &= static_cast<std::uint8_t>(~kPaused); });
}

void TweenSet::kill_all() {
    detail::for_each_active(block_, detail::tween_kill_slot);
}

void TweenSet::kill_target(const void* target) {
    if (target == nullptr) return;
    detail::for_each_active(block_, [target](detail::TweenBlock& b, int i) {
        if (b.hot[i].target == target) detail::tween_kill_slot(b, i);
    });
}

} // namespace nnh
