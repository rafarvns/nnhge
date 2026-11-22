// The animator's state machine (spec 016 FR-12, FR-13; T5.2): request with the rule table (now, at the end of the
// clip or loop, never; keep-time), force, switches as commands or during an update, and the state queries.

#include "animation/animator_internal.hpp"
#include "animation/sprite_sequence.hpp"

namespace nnh {

using namespace detail;
using namespace detail::spritefmt;
using Access = SpriteAnimatorAccess;

void Access::switch_command(SpriteAnimator& a, std::uint8_t state, bool keep_time) {
    const SpriteView v = sprite_view(a.block_);
    const int clip = v.states()[state].clip;
    const int step = a.step_;
    const std::int32_t elapsed = a.elapsed_;
    const bool had_clip = a.clip_ != kNoClip;
    a.previous_ = a.state_;
    a.state_ = state;
    a.state_time_ = 0;
    a.queue_size_ = 0;
    start_command(a, clip, 0);
    a.flags_ = static_cast<std::uint8_t>(a.flags_ | kAnimStatePending);
    if (keep_time && had_clip) {
        const ClipEntry& c = v.clips()[clip];
        const int length = sprite_cycle_length(c.length, c.mode);
        a.step_ = static_cast<std::uint16_t>(step < length ? step : length - 1);
        const std::int32_t duration = clip_frame(a).duration;
        a.elapsed_ = elapsed < duration ? elapsed : duration - 1;
    }
}

void Access::switch_now(SpriteAnimator& a, Pass& p, std::uint8_t state) {
    a.previous_ = a.state_;
    a.state_ = state;
    a.state_time_ = a.elapsed_; // the time carried into the new state is spent in it
    a.clip_ = sprite_view(a.block_).states()[state].clip;
    a.step_ = 0;
    a.cycles_ = 0;
    a.flags_ = static_cast<std::uint8_t>(a.flags_ & ~kAnimFinished);
    emit_simple(a, p, SpriteNoticeKind::StateChanged);
    if (!changed(a, p)) emit_markers(a, p);
}

void SpriteAnimator::request(SpriteStateId state) {
    const SpriteView v = Access::library(*this, "request");
    if (v.block == nullptr) return;
    if (state.value >= v.count(kStates)) {
        Access::misuse("request: not a state of the library");
        return;
    }
    if (state_ == kNoState) {
        force(state);
        return;
    }
    if (state.value == state_) {
        pending_ = kNoState; // the game no longer wants to leave
        return;
    }
    if (state.value == pending_) return;
    const TransitionEntry rule = sprite_resolve_rule(v.transitions(), v.count(kTransitions), state_, state.value);
    if (rule.when == kNever) return;
    if (rule.when == kAtEnd && (flags_ & kAnimFinished) == 0) {
        pending_ = state.value;
        return;
    }
    pending_ = kNoState;
    Access::switch_command(*this, state.value, rule.when == kNow && rule.keep_time != 0);
}

void SpriteAnimator::force(SpriteStateId state) {
    const SpriteView v = Access::library(*this, "force");
    if (v.block == nullptr) return;
    if (state.value >= v.count(kStates)) {
        Access::misuse("force: not a state of the library");
        return;
    }
    pending_ = kNoState;
    if (state.value == state_) return;
    Access::switch_command(*this, state.value, false);
}

SpriteStateId SpriteAnimator::state() const {
    return SpriteStateId{state_};
}

SpriteStateId SpriteAnimator::previous_state() const {
    return SpriteStateId{previous_};
}

FrameTime SpriteAnimator::time_in_state() const {
    return state_ == kNoState ? FrameTime{} : FrameTime::from_ticks(state_time_);
}

} // namespace nnh
