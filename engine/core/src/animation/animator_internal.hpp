#pragma once
// The animator's internals (spec 016), shared by animator*.cpp: flags, the module's notice scratch and the steps of
// an update. SpriteAnimatorAccess is the animator's friend; its functions are the module's internal API.

#include <cstdint>

#include "animation/sprite_library_internal.hpp"
#include "animation/sprite_sequence.hpp"

namespace nnh {

namespace detail {

inline constexpr std::uint8_t kAnimPaused = 1;
inline constexpr std::uint8_t kAnimFinished = 2;
inline constexpr std::uint8_t kAnimEntryPending = 4; ///< the current frame was entered by a command
inline constexpr std::uint8_t kAnimStatePending = 8; ///< ... and by a state change (StateChanged first)
inline constexpr std::uint8_t kAnimFlipX = 16;
inline constexpr std::uint8_t kAnimFlipY = 32;
inline constexpr std::uint16_t kNoClip = 0xFFFF;

/// Silent steps (after the crossing bound) one update may take before it drops the rest of its time.
inline constexpr int kMaxSilentSteps = 1 << 16;

/// The notices of the running update (one module-wide buffer: animators are single-threaded).
struct NoticeScratch {
    SpriteNotice items[kSpriteMaxNotices];
    int count = 0;
    int callbacks_running = 0; ///< > 0 while a callback runs: update()/advance() then is misuse
};
extern NoticeScratch g_notice_scratch; // animator_notices.cpp
inline NoticeScratch& notice_scratch() {
    return g_notice_scratch;
}

/// State of one pass of an update over its animator.
struct AnimPass {
    std::uint8_t changes = 0; ///< the animator's change counter when the pass started
    int crossings = 0;
    int silent_steps = 0;
    bool silent = false; ///< past a bound: no more notices or callbacks in this update
};

/// After an expected assert inside an update (tests): the module is idle again.
void animator_recover_after_crash();

} // namespace detail

struct SpriteAnimatorAccess {
    using Pass = detail::AnimPass;

    // Library access and misuse checks (animator.cpp).
    /// The animator's library, or a null view (with an assert/log when `command` and there is none, or when the
    /// library was destroyed: Debug/Development).
    /// Inline (every command and update checks): the misuse cases go to library_misuse().
    static detail::SpriteView library(const SpriteAnimator& a, const char* command) {
        if (a.block_ != nullptr && detail::sprite_serial_live(a.serial_)) {
#if !defined(NNH_FLAVOR_RELEASE)
            // Spec 031: the library was reloaded since this animator last ran: rebind first (queries too: an
            // animator is never a const object in practice; its fields are only re-pointed at the new content).
            if (detail::g_sprite_reloads != 0) rebind_if_reloaded(const_cast<SpriteAnimator&>(a));
#endif
            return detail::sprite_view(a.block_);
        }
        return library_misuse(a, command);
    }
    /// Spec 031 FR-8 (animator_rebind.cpp; Debug/Development): when the animator's block isn't its library's current
    /// block, moves it onto the current one: the clip of the same name at its frame (clamped), else the first clip
    /// with a warning; a state machine whose states or rules changed restarts at state 0 with a warning. The old
    /// block is read only when it is still alive (the library's identity or previous block), else never.
    static void rebind_if_reloaded(SpriteAnimator& a);
    /// The queries that read the animator's own fields call this first (Debug/Development: the rebind above).
    static void fresh(const SpriteAnimator& a) {
#if !defined(NNH_FLAVOR_RELEASE)
        if (detail::g_sprite_reloads != 0 && a.block_ != nullptr && detail::sprite_serial_live(a.serial_)) {
            rebind_if_reloaded(const_cast<SpriteAnimator&>(a));
        }
#else
        (void)a;
#endif
    }
    static detail::SpriteView library_misuse(const SpriteAnimator& a, const char* command);
    static bool misuse(const char* what); ///< logs and asserts `what`; returns false
    static const detail::spritefmt::ClipEntry& clip(const SpriteAnimator& a) {
        return detail::sprite_view(a.block_).clips()[a.clip_];
    }
    /// The clip frame shown (inline: every update reads its duration).
    static const detail::spritefmt::ClipFrameEntry& clip_frame(const SpriteAnimator& a) {
        const detail::SpriteView v = detail::sprite_view(a.block_);
        const detail::spritefmt::ClipEntry& c = v.clips()[a.clip_];
        return v.clip_frames()[c.first + detail::sprite_clip_index(c.length, c.mode, c.direction, a.step_)];
    }
    /// Starts `clip` at cycle position `step` as a command (entry fires at the next update).
    static void start_command(SpriteAnimator& a, int clip, int step);
    /// Leaves the state machine (play, queue).
    static void leave_machine(SpriteAnimator& a);

    // States (animator_state.cpp).
    /// Switches to `state` as a command: entry and StateChanged fire at the next update.
    static void switch_command(SpriteAnimator& a, std::uint8_t state, bool keep_time);
    /// Switches during an update, carrying the time: emits StateChanged and the entry at once.
    static void switch_now(SpriteAnimator& a, Pass& p, std::uint8_t state);

    // Notices (animator_notices.cpp).
    /// Appends `n` and calls the callback (nothing once silent). Past kSpriteMaxNotices: the bound.
    static void emit(SpriteAnimator& a, Pass& p, const SpriteNotice& n);
    /// The markers of the current frame, in order.
    static void emit_markers(SpriteAnimator& a, Pass& p);
    static void emit_simple(SpriteAnimator& a, Pass& p, SpriteNoticeKind kind);
    static void report_bound(Pass& p);
    static bool changed(const SpriteAnimator& a, const Pass& p) { return a.changes_ != p.changes; }

    // Time (animator_advance.cpp, animator_walk.cpp).
    static SpriteNotices run(SpriteAnimator& a, std::int32_t ticks);
    /// Fires what the last commands entered: StateChanged, then the frame's markers.
    static void fire_entry(SpriteAnimator& a, Pass& p);
    /// Crosses every frame whose time is used up; stops when a callback changes the clip.
    static void walk(SpriteAnimator& a, Pass& p);
    /// Leaves the current position: the next one, a wrap, or the end of the clip.
    static void cross(SpriteAnimator& a, Pass& p);
    /// At the end of a clip (`finished`) or of a loop: the queue, a request waiting for the end, then (finished)
    /// next-on-finish. True when it switched (the new clip's entry was emitted).
    static bool end_switch(SpriteAnimator& a, Pass& p, bool finished);
    /// Silent (past a bound): at the start of a cycle, skips whole cycles where nothing can switch.
    static void skip_cycles(SpriteAnimator& a);
};

} // namespace nnh
