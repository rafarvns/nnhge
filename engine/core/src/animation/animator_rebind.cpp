// An animator meets a reloaded library (spec 031 FR-8; Debug/Development): rebind_if_reloaded moves it onto the
// library's current block before any other work. The old block is compared by pointer and read only when it is still
// alive (the library's identity block or its previous one); one freed meanwhile (two reloads before this animator ran)
// is never read: the animator then starts the first clip (or state 0) with a warning. Tests:
// tests/animation/test_animator_rebind.cpp.

#include <cstring>

#include "animation/animator_internal.hpp"
#include "animation/sprite_sequence.hpp"

namespace nnh {

using namespace detail;
using namespace detail::spritefmt;
using Access = SpriteAnimatorAccess;

namespace {

/// The same states (names, clips by name, next on finish) and the same rules, byte for byte.
bool same_machine(const SpriteView& o, const SpriteView& n) {
    if (o.count(kStates) != n.count(kStates) || o.count(kTransitions) != n.count(kTransitions)) return false;
    for (int s = 0; s < o.count(kStates); ++s) {
        const StateEntry& a = o.states()[s];
        const StateEntry& b = n.states()[s];
        if (a.hash != b.hash || a.next != b.next || o.clips()[a.clip].hash != n.clips()[b.clip].hash) return false;
    }
    return std::memcmp(o.transitions(), n.transitions(),
                       sizeof(TransitionEntry) * static_cast<std::size_t>(o.count(kTransitions))) == 0;
}

int clip_by_hash(const SpriteView& n, std::uint32_t hash) {
    for (int c = 0; c < n.count(kClips); ++c) {
        if (n.clips()[c].hash == hash) return c;
    }
    return -1;
}

/// Keeps the position in the cycle inside the new clip, and the time inside the new frame's duration.
void clamp_into(const SpriteView& n, std::uint16_t clip, std::uint16_t& step, std::int32_t& elapsed) {
    const ClipEntry& c = n.clips()[clip];
    const int length = sprite_cycle_length(c.length, c.mode);
    if (step >= length) step = static_cast<std::uint16_t>(length - 1);
    const int index = sprite_clip_index(c.length, c.mode, c.direction, step);
    const std::int32_t duration = n.clip_frames()[c.first + index].duration;
    if (elapsed >= duration) elapsed = duration - 1;
}

} // namespace

void Access::rebind_if_reloaded(SpriteAnimator& a) {
    SpriteReloadEntry* e = sprite_reload_entry_of_serial(a.serial_);
    if (e == nullptr) return;
    const bool bound = e->current == a.block_ && a.reload_epoch_ == e->epoch;
    // Bound to the current block: nothing to do, unless its clip is outside the block (never expected; review S4: the
    // animator must not read past the clips whatever happened).
    if (bound && (a.clip_ == kNoClip || a.clip_ < SpriteView{e->current}.count(kClips))) return;
    // Never dereference anything but the identity block or the previous one bound in the previous epoch (a freed
    // block's address may come back as a later block: the full 16-bit epoch tells them apart).
    const bool readable = !bound && (a.block_ == e->identity ||
                                     (a.block_ == e->previous && a.reload_epoch_ == static_cast<std::uint16_t>(e->epoch - 1)));
    const SpriteView o{readable ? const_cast<SpriteBlock*>(a.block_) : nullptr};
    const SpriteView n{e->current};
    a.block_ = e->current;
    a.reload_epoch_ = e->epoch;
    if (a.state_ != kNoState) {
        if (o.block != nullptr && same_machine(o, n)) {
            // the states' clips are the same records: the playing clip is remapped by name below
        } else {
            NNH_LOG_WARN(animation, "animation: the state machine of %s changed: back to state \"%s\"", e->name,
                         n.count(kStates) > 0 ? n.states()[0].name : "");
            leave_machine(a);
            a.queue_size_ = 0;
            if (n.count(kStates) > 0) {
                a.state_ = 0;
                start_command(a, n.states()[0].clip, 0);
            } else {
                a.clip_ = kNoClip;
            }
            return;
        }
    }
    // The queue: the same clips by name when the old block can be read, else dropped.
    int kept = 0;
    for (int q = 0; q < a.queue_size_; ++q) {
        const bool known = o.block != nullptr && a.queue_[q] < o.count(kClips);
        const int c = known ? clip_by_hash(n, o.clips()[a.queue_[q]].hash) : -1;
        if (c >= 0) a.queue_[kept++] = static_cast<std::uint8_t>(c);
    }
    a.queue_size_ = static_cast<std::uint8_t>(kept);
    if (a.clip_ == kNoClip) return;
    const int found = o.block != nullptr && a.clip_ < o.count(kClips) ? clip_by_hash(n, o.clips()[a.clip_].hash) : -1;
    if (found >= 0) {
        a.clip_ = static_cast<std::uint16_t>(found);
        clamp_into(n, a.clip_, a.step_, a.elapsed_);
        return;
    }
    if (n.count(kClips) == 0) {
        a.clip_ = kNoClip;
        return;
    }
    NNH_LOG_WARN(animation, "animation: clip \"%s\" no longer exists in %s: playing \"%s\"",
                 o.block != nullptr && a.clip_ < o.count(kClips) ? o.clips()[a.clip_].name : "?", e->name,
                 n.clips()[0].name);
    start_command(a, 0, 0);
}

} // namespace nnh
