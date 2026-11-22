// The animator's commands (spec 016 FR-5, FR-6; T3.1): construction, play/queue/stop/pause/set_frame/speed/flip,
// the callback, and the library and lifetime checks every command makes.

#include <utility>

#include "animation/animator_internal.hpp"
#include "animation/sprite_sequence.hpp"
#include "nnh/assert.hpp"

namespace nnh {

using namespace detail;
using namespace detail::spritefmt;
using Access = SpriteAnimatorAccess;

bool Access::misuse(const char* what) {
    NNH_LOG_ERROR(animation, "SpriteAnimator: %s (ignored)", what);
    NNH_ASSERT(false, "nnh::SpriteAnimator: %s", what);
    return false;
}

SpriteView Access::library(const SpriteAnimator& a, const char* command) {
    if (a.block_ == nullptr) {
        if (command != nullptr) misuse("command on an animator without a library");
        return SpriteView{nullptr};
    }
    if (!sprite_serial_live(a.serial_)) {
        misuse("its SpriteLibrary was destroyed (a library must outlive its animators)");
        return SpriteView{nullptr};
    }
    return sprite_view(a.block_);
}

const ClipEntry& Access::clip(const SpriteAnimator& a) {
    return sprite_view(a.block_).clips()[a.clip_];
}

const ClipFrameEntry& Access::clip_frame(const SpriteAnimator& a) {
    const ClipEntry& c = clip(a);
    const int index = sprite_clip_index(c.length, c.mode, c.direction, a.step_);
    return sprite_view(a.block_).clip_frames()[c.first + index];
}

void Access::start_command(SpriteAnimator& a, int clip, int step) {
    a.clip_ = static_cast<std::uint16_t>(clip);
    a.step_ = static_cast<std::uint16_t>(step);
    a.cycles_ = 0;
    a.elapsed_ = 0;
    a.flags_ = static_cast<std::uint8_t>((a.flags_ & ~(kAnimFinished | kAnimStatePending)) | kAnimEntryPending);
    ++a.changes_;
}

void Access::leave_machine(SpriteAnimator& a) {
    a.state_ = kNoState;
    a.pending_ = kNoState;
    a.state_time_ = 0;
}

namespace {

/// The first cycle position showing clip frame `position`.
int step_of(const ClipEntry& c, int position) {
    const int length = sprite_cycle_length(c.length, c.mode);
    for (int s = 0; s < length; ++s) {
        if (sprite_clip_index(c.length, c.mode, c.direction, s) == position) return s;
    }
    return 0;
}

} // namespace

SpriteAnimator::SpriteAnimator(const SpriteLibrary& library) {
    block_ = SpriteLibraryAccess::block(library);
    if (block_ == nullptr) {
        Access::misuse("built from an empty SpriteLibrary");
        return;
    }
    serial_ = block_->header.serial;
}

SpriteAnimator::SpriteAnimator(const SpriteLibrary& library, SpriteStateId start) : SpriteAnimator(library) {
    const SpriteView v = Access::library(*this, "SpriteAnimator(library, state)");
    if (v.block == nullptr) return;
    if (start.value >= v.count(kStates)) {
        Access::misuse("start state is not a state of the library");
        return;
    }
    state_ = start.value;
    Access::start_command(*this, v.states()[start.value].clip, 0);
}

void SpriteAnimator::play(SpriteClipId clip, bool restart) {
    const SpriteView v = Access::library(*this, "play");
    if (v.block == nullptr) return;
    if (clip.value >= v.count(kClips)) {
        Access::misuse("play: not a clip of the library");
        return;
    }
    Access::leave_machine(*this);
    queue_size_ = 0;
    if (!restart && clip_ == clip.value && (flags_ & kAnimFinished) == 0) return;
    Access::start_command(*this, clip.value, 0);
}

void SpriteAnimator::play(const char* name, bool restart) {
    const SpriteView v = Access::library(*this, "play");
    if (v.block == nullptr) return;
    const SpriteClipId clip = sprite_find_clip(v, name);
    if (!clip.valid()) {
        Access::misuse("play: no clip has this name");
        return;
    }
    play(clip, restart);
}

void SpriteAnimator::play_from(SpriteClipId clip, int position) {
    const SpriteView v = Access::library(*this, "play_from");
    if (v.block == nullptr) return;
    if (clip.value >= v.count(kClips) || position < 0 || position >= v.clips()[clip.value].length) {
        Access::misuse("play_from: not a clip of the library, or no such frame in it");
        return;
    }
    Access::leave_machine(*this);
    queue_size_ = 0;
    Access::start_command(*this, clip.value, step_of(v.clips()[clip.value], position));
}

Result<void> SpriteAnimator::queue(SpriteClipId clip) {
    const SpriteView v = Access::library(*this, "queue");
    if (v.block == nullptr) return Error::make(ErrorCode::InvalidArgument, "queue: no library");
    if (clip.value >= v.count(kClips)) {
        Access::misuse("queue: not a clip of the library");
        return Error::make(ErrorCode::InvalidArgument, "queue: not a clip of the library");
    }
    if (clip_ == kNoClip || (flags_ & kAnimFinished) != 0) {
        play(clip, true);
        return {};
    }
    if (queue_size_ >= kSpriteMaxQueue) return Error::make(ErrorCode::LimitReached, "queue: 4 clips wait already");
    Access::leave_machine(*this);
    queue_[queue_size_++] = static_cast<std::uint8_t>(clip.value);
    return {};
}

void SpriteAnimator::clear_queue() {
    queue_size_ = 0;
}

void SpriteAnimator::stop() {
    if (Access::library(*this, "stop").block == nullptr) return;
    Access::leave_machine(*this);
    queue_size_ = 0;
    clip_ = kNoClip;
    elapsed_ = 0;
    flags_ = static_cast<std::uint8_t>(flags_ & ~(kAnimFinished | kAnimEntryPending | kAnimStatePending));
    ++changes_;
}

void SpriteAnimator::pause() {
    flags_ = static_cast<std::uint8_t>(flags_ | kAnimPaused);
}

void SpriteAnimator::resume() {
    flags_ = static_cast<std::uint8_t>(flags_ & ~kAnimPaused);
}

void SpriteAnimator::set_frame(int position) {
    const SpriteView v = Access::library(*this, "set_frame");
    if (v.block == nullptr) return;
    if (clip_ == kNoClip || position < 0 || position >= v.clips()[clip_].length) {
        Access::misuse("set_frame: no clip, or no such frame in it");
        return;
    }
    Access::start_command(*this, clip_, step_of(v.clips()[clip_], position));
}

void SpriteAnimator::set_speed(float speed) {
    if (!(speed >= 0.0f && speed <= 64.0f)) {
        Access::misuse("set_speed: the speed is outside 0..64");
        return;
    }
    speed_ = static_cast<std::int32_t>(speed * FrameTime::kTicksPerFrame + 0.5f);
}

float SpriteAnimator::speed() const {
    return static_cast<float>(speed_) / static_cast<float>(FrameTime::kTicksPerFrame);
}

void SpriteAnimator::set_flip(bool flip_x, bool flip_y) {
    flags_ = static_cast<std::uint8_t>((flags_ & ~(kAnimFlipX | kAnimFlipY)) | (flip_x ? kAnimFlipX : 0) |
                                       (flip_y ? kAnimFlipY : 0));
}

void SpriteAnimator::on_notice(SpriteCallback callback) {
    callback_ = callback;
}

} // namespace nnh
