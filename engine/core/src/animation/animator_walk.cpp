// Frame crossings of an update (spec 016 FR-4, FR-7, FR-13; T3.2): the leftover time carried, wraps and ends of
// clips (queue, a request waiting for the end, next-on-finish, hold) and the crossing bound.

#include "animation/animator_internal.hpp"
#include "animation/sprite_sequence.hpp"

namespace nnh {

using namespace detail;
using namespace detail::spritefmt;
using Access = SpriteAnimatorAccess;

namespace {

bool finite(const ClipEntry& c) {
    return c.mode != kLoop && c.count != 0;
}

} // namespace

void Access::fire_entry(SpriteAnimator& a, Pass& p) {
    const bool state_changed = (a.flags_ & kAnimStatePending) != 0;
    a.flags_ = static_cast<std::uint8_t>(a.flags_ & ~(kAnimEntryPending | kAnimStatePending));
    if (state_changed) {
        emit_simple(a, p, SpriteNoticeKind::StateChanged);
        if (changed(a, p)) return;
    }
    emit_markers(a, p);
}

bool Access::end_switch(SpriteAnimator& a, Pass& p, bool finished) {
    if (a.queue_size_ > 0) {
        a.clip_ = a.queue_[0];
        for (int i = 1; i < a.queue_size_; ++i) a.queue_[i - 1] = a.queue_[i];
        --a.queue_size_;
        a.step_ = 0;
        a.cycles_ = 0;
        a.flags_ = static_cast<std::uint8_t>(a.flags_ & ~kAnimFinished);
        emit_markers(a, p);
        return true;
    }
    if (a.pending_ != kNoState) {
        const std::uint8_t state = a.pending_;
        a.pending_ = kNoState;
        switch_now(a, p, state);
        return true;
    }
    if (finished && a.state_ != kNoState) {
        const std::uint8_t next = sprite_view(a.block_).states()[a.state_].next;
        if (next != kNoState) {
            switch_now(a, p, next);
            return true;
        }
    }
    return false;
}

void Access::cross(SpriteAnimator& a, Pass& p) {
    const ClipEntry& c = clip(a);
    if (finite(c) && a.cycles_ == c.end_cycle && a.step_ == c.end_step) {
        const std::int32_t held = clip_frame(a).duration;
        emit_simple(a, p, SpriteNoticeKind::Finished);
        if (changed(a, p) || end_switch(a, p, true)) return;
        a.flags_ = static_cast<std::uint8_t>(a.flags_ | kAnimFinished);
        a.elapsed_ = held; // holds the last frame, its time full
        return;
    }
    if (++a.step_ == sprite_cycle_length(c.length, c.mode)) {
        a.step_ = 0;
        if (a.cycles_ < 0xFFFF) ++a.cycles_;
        emit_simple(a, p, SpriteNoticeKind::Looped);
        if (changed(a, p) || end_switch(a, p, false)) return;
    }
    emit_markers(a, p);
}

void Access::skip_cycles(SpriteAnimator& a) {
    const ClipEntry& c = clip(a);
    if (a.step_ != 0 || a.elapsed_ < c.cycle_ticks || a.queue_size_ > 0 || a.pending_ != kNoState) return;
    std::int32_t whole = a.elapsed_ / c.cycle_ticks;
    if (finite(c)) {
        const std::int32_t left = c.end_cycle > a.cycles_ ? c.end_cycle - a.cycles_ : 0;
        whole = whole < left ? whole : left;
    }
    a.elapsed_ -= whole * c.cycle_ticks;
    const std::int32_t cycles = a.cycles_ + whole;
    a.cycles_ = static_cast<std::uint16_t>(cycles < 0xFFFF ? cycles : 0xFFFF);
}

void Access::walk(SpriteAnimator& a, Pass& p) {
    while ((a.flags_ & (kAnimPaused | kAnimFinished)) == 0 && a.clip_ != kNoClip) {
        if (p.silent) skip_cycles(a);
        const std::int32_t duration = clip_frame(a).duration;
        if (a.elapsed_ < duration) return;
        if (!p.silent && p.crossings == kSpriteMaxCrossings) {
            report_bound(p);
            continue;
        }
        if (p.silent && ++p.silent_steps > kMaxSilentSteps) {
            a.elapsed_ = 0; // pathological data (chains of tiny clips): drop the rest
            return;
        }
        ++p.crossings;
        a.elapsed_ -= duration;
        cross(a, p);
        if (changed(a, p)) return;
    }
}

} // namespace nnh
