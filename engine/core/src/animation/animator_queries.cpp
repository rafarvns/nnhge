// The animator's queries (spec 016 FR-6, FR-10; T3.3): clip, position, sheet frame, region, pivot with offset and
// flips, times, progress, and draw_options().

#include "animation/animator_internal.hpp"
#include "animation/sprite_frame_math.hpp"
#include "animation/sprite_sequence.hpp"
#include "nnh/gfx.hpp"

namespace nnh {

using namespace detail;
using namespace detail::spritefmt;
using Access = SpriteAnimatorAccess;

namespace {

/// The sheet frame shown, or nullptr with no clip.
const FrameEntry* shown_frame(const SpriteAnimator& a, int clip) {
    if (clip == kNoClip) return nullptr;
    const SpriteView v = Access::library(a, nullptr);
    if (v.block == nullptr) return nullptr;
    return &v.frames()[Access::clip_frame(a).frame];
}

} // namespace

bool SpriteAnimator::playing() const {
    return clip_ != kNoClip && (flags_ & kAnimFinished) == 0;
}
bool SpriteAnimator::paused() const {
    return (flags_ & kAnimPaused) != 0;
}
bool SpriteAnimator::finished() const {
    return clip_ != kNoClip && (flags_ & kAnimFinished) != 0;
}
SpriteClipId SpriteAnimator::clip() const {
    return SpriteClipId{clip_};
}

const char* SpriteAnimator::clip_name() const {
    if (clip_ == kNoClip || Access::library(*this, nullptr).block == nullptr) return "";
    return Access::clip(*this).name;
}

int SpriteAnimator::position() const {
    if (clip_ == kNoClip || Access::library(*this, nullptr).block == nullptr) return -1;
    const ClipEntry& c = Access::clip(*this);
    return sprite_clip_index(c.length, c.mode, c.direction, step_);
}

int SpriteAnimator::frame() const {
    if (clip_ == kNoClip || Access::library(*this, nullptr).block == nullptr) return -1;
    return Access::clip_frame(*this).frame;
}

Region SpriteAnimator::region() const {
    const FrameEntry* e = shown_frame(*this, clip_);
    return e != nullptr ? Region{e->x, e->y, e->w, e->h} : Region{};
}

Vec2 SpriteAnimator::pivot() const {
    const FrameEntry* e = shown_frame(*this, clip_);
    return e != nullptr ? sprite_draw_pivot(*e, flip_x(), flip_y()) : Vec2{};
}

Vec2 SpriteAnimator::offset() const {
    const FrameEntry* e = shown_frame(*this, clip_);
    return e != nullptr ? sprite_frame_of(*e).offset : Vec2{};
}

FrameTime SpriteAnimator::time_in_frame() const {
    return clip_ == kNoClip ? FrameTime{} : FrameTime::from_ticks(elapsed_);
}

float SpriteAnimator::progress() const {
    if (clip_ == kNoClip || Access::library(*this, nullptr).block == nullptr) return 0.0f;
    if ((flags_ & kAnimFinished) != 0) return 1.0f;
    const ClipEntry& c = Access::clip(*this);
    const SpriteView v = sprite_view(block_);
    // Time into the current cycle: the positions before this one, plus the time in this frame.
    std::int64_t into = elapsed_;
    for (int s = 0; s < step_; ++s) {
        into += v.clip_frames()[c.first + sprite_clip_index(c.length, c.mode, c.direction, s)].duration;
    }
    const bool finite = c.mode != kLoop && c.count != 0;
    if (!finite) return static_cast<float>(into) / static_cast<float>(c.cycle_ticks);
    std::int64_t last = 0; // the end cycle's time up to the end of its last position
    for (int s = 0; s <= c.end_step; ++s) {
        last += v.clip_frames()[c.first + sprite_clip_index(c.length, c.mode, c.direction, s)].duration;
    }
    const std::int64_t total = static_cast<std::int64_t>(c.end_cycle) * c.cycle_ticks + last;
    const std::int64_t done = static_cast<std::int64_t>(cycles_) * c.cycle_ticks + into;
    return static_cast<float>(static_cast<double>(done) / static_cast<double>(total));
}

int SpriteAnimator::loops() const {
    return cycles_;
}

bool SpriteAnimator::flip_x() const {
    return (flags_ & kAnimFlipX) != 0;
}
bool SpriteAnimator::flip_y() const {
    return (flags_ & kAnimFlipY) != 0;
}

DrawOptions SpriteAnimator::draw_options() const {
    DrawOptions o;
    const FrameEntry* e = shown_frame(*this, clip_);
    if (e == nullptr) return o;
    o.region = {e->x, e->y, e->w, e->h};
    o.pivot = sprite_draw_pivot(*e, flip_x(), flip_y());
    o.flip_x = flip_x();
    o.flip_y = flip_y();
    return o;
}

} // namespace nnh
