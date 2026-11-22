// update()/advance() (spec 016 FR-7..FR-9; T3.2, T4.3): the pending entry, time × speed with the remainder carried,
// the walk over frame crossings (animator_walk.cpp), and the re-entrancy depth.

#include "animation/animator_internal.hpp"

namespace nnh {

using namespace detail;
using namespace detail::spritefmt;
using Access = SpriteAnimatorAccess;

namespace {

std::int32_t saturated_add(std::int32_t a, std::int32_t b) {
    return a > kMaxTicks - b ? kMaxTicks : a + b;
}

} // namespace

SpriteNotices Access::run(SpriteAnimator& a, std::int32_t ticks) {
    NoticeScratch& s = notice_scratch();
    if (s.callbacks_running > 0) {
        misuse("update()/advance() called from a notice callback");
        return {};
    }
    s.count = 0;
    if (library(a, nullptr).block == nullptr || (a.flags_ & kAnimPaused) != 0 || a.clip_ == kNoClip) {
        return {s.items, 0};
    }
    Pass p;
    bool time_used = false; // added once; a callback that changes the clip drops the rest of it
    for (int depth = 0;;) {
        p.changes = a.changes_;
        if ((a.flags_ & kAnimEntryPending) != 0) fire_entry(a, p);
        if (changed(a, p)) {
            time_used = true;
        } else {
            if (!time_used) {
                time_used = true;
                if (a.state_ != kNoState) a.state_time_ = saturated_add(a.state_time_, ticks);
                if ((a.flags_ & kAnimFinished) == 0) a.elapsed_ = saturated_add(a.elapsed_, ticks);
            }
            walk(a, p);
            if (!changed(a, p)) break;
        }
        if (++depth >= kSpriteMaxCallbackDepth) {
            NNH_LOG_ERROR(animation, "SpriteAnimator: callbacks changed the clip %d times in one update (stopped; "
                                     "the last clip plays from the next update)",
                          depth);
            break;
        }
    }
    return {s.items, s.count};
}

SpriteNotices SpriteAnimator::update() {
    return Access::run(*this, speed_);
}

SpriteNotices SpriteAnimator::advance(FrameTime time) {
    if (!time.valid()) {
        Access::misuse("advance: the time is negative or over 1 hour");
        return {};
    }
    // time × speed / 4096, the remainder kept for the next call: no error accumulates.
    const std::int64_t scaled = static_cast<std::int64_t>(time.ticks) * speed_ + remainder_;
    remainder_ = static_cast<std::uint16_t>(scaled % FrameTime::kTicksPerFrame);
    const std::int64_t ticks = scaled / FrameTime::kTicksPerFrame;
    return Access::run(*this, static_cast<std::int32_t>(ticks < kMaxTicks ? ticks : kMaxTicks));
}

} // namespace nnh
