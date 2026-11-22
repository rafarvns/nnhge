#pragma once
/// @file nnh/tween.hpp
/// Tweens and timers (spec 015): easing curves, a plain timer value, and tween sets that animate a float, Vec2
/// or Color of the game over a span of the deterministic frame clock. See docs/api/tween.md.
///
/// Optional (constitution XI): a game that doesn't include this header links none of it. The easing curves and
/// nnh::Timer work alone, with no set, no scene and no nnh::init().

#include <cstdint>
#include <type_traits>

#include "nnh/detail/tween_detail.hpp"
#include "nnh/time.hpp"

namespace nnh {

// ---- Easing (FR-1..FR-3) ----

/// The standard easing curves (Robert Penner's equations): back overshoots by 1.70158, elastic uses period 0.3
/// (0.45 for in-out), bounce has four bounces.
enum class Ease : std::uint8_t {
    Linear,
    QuadIn, QuadOut, QuadInOut,
    CubicIn, CubicOut, CubicInOut,
    SineIn, SineOut, SineInOut,
    BackIn, BackOut, BackInOut,
    ElasticIn, ElasticOut, ElasticInOut,
    BounceIn, BounceOut, BounceInOut,
};
inline constexpr int kEaseCount = 19;

/// A game's own curve: progress in [0, 1] → eased value.
using EaseFn = float (*)(float t);

/// Evaluates `curve` at progress `t`. Exactly 0 for t ≤ 0, NaN and 0 < t < 2⁻²⁴; exactly 1 for t ≥ 1. Back and
/// elastic leave [0, 1] in between; the others stay inside it. Same bits on every compiler and system (no libm:
/// own approximations, within 2⁻¹² of the exact curve; see docs/api/tween.md). Pure: no state, no init needed,
/// allocates nothing.
/// @code
/// const float x = 10 + 200 * nnh::ease(nnh::Ease::CubicOut, t);
/// @endcode
float ease(Ease curve, float t);

/// A built-in curve or the game's own function, accepted wherever a curve is: `.ease = nnh::Ease::BackOut` or
/// `.ease = my_curve`. Custom functions are outside the determinism guarantee (the game's own float code).
struct Easing {
    // NOLINTNEXTLINE(google-explicit-constructor): implicit on purpose.
    constexpr Easing(Ease e = Ease::Linear) : builtin(e) {}
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr Easing(EaseFn f) : custom(f) {}

    /// custom ? custom(t) : ease(builtin, t).
    float operator()(float t) const;

    Ease builtin = Ease::Linear;
    EaseFn custom = nullptr;
};

// ---- The timer value (FR-14) ----

/// A plain timer the game advances itself (Godot's Timer as a struct, 12 bytes): one-shot or repeating, on the
/// deterministic frame clock. No set, no scene, no nnh::init(), no allocation. Prefer it when you'd rather query
/// than get a callback.
/// @code
/// nnh::Timer shot_ = nnh::Timer::repeating(nnh::Duration::from_ms(250));
/// for (int n = shot_.update(); n > 0; --n) fire(); // update() returns how many periods ended this frame
/// @endcode
/// Misuse (a negative or over-1-hour time, a repeating period of 0) asserts in Debug/Development; in Release it is
/// logged and the timer stays stopped (or the advance is ignored).
class Timer {
public:
    Timer() = default; ///< stopped; never fires until restart() (then fires on the next update)

    /// Running; fires once when `time` has passed (a time of 0 fires on the first update), then stops.
    static Timer once(FrameTime time);
    /// Running; fires every `period` (≥ 1 tick) forever.
    static Timer repeating(FrameTime period);

    /// Advances one 60 Hz frame. @return how many times the timer fired: 0, 1, or more for a repeating timer
    /// whose period is shorter than a frame. 0 while stopped or paused.
    int update();
    /// Advances any span (from time::FixedStep, to skip ahead...). Every period that ends counts; the leftover
    /// carries into the next period, so a repeating timer never drifts. A one-shot timer fires at most once.
    int advance(FrameTime time);

    bool running() const; ///< started and not finished or stopped (true while paused)
    bool paused() const;
    FrameTime elapsed() const;   ///< into the current period (the whole time once a one-shot timer fired)
    FrameTime remaining() const; ///< until the next fire
    float progress() const;      ///< elapsed / period, 0..1 (a fired zero-length one-shot: 1)

    void pause();   ///< keeps the time; update() returns 0 until resume()
    void resume();
    void stop();    ///< halts and rewinds to 0 (remaining() is the whole period again)
    void restart(); ///< rewinds to 0 and runs, unpaused, with the same period

private:
    std::int32_t period_ = 0;
    std::int32_t elapsed_ = 0;
    std::uint8_t flags_ = 0;
};

// ---- Tween sets (FR-4..FR-13, FR-16..FR-18) ----

/// What happens at the end of each loop but the last: jump back to the start value, or play backwards with the
/// mirrored easing (each direction counts as one loop).
enum class LoopMode : std::uint8_t { Restart, Yoyo };

/// A loop count meaning "never finish" (only pause, kill or clearing the set stop it).
inline constexpr int kForever = -1;

/// A callback stored inline (no allocation): a function pointer or a lambda with at most 16 bytes of trivially
/// copyable captures (`[this]`, `[this, id]`).
using TweenCallback = detail::SmallFunction<void()>;
/// A copy of one of the game's own events, published on finish: `.publish = IntroFinished{}` (≤ 32 bytes).
using FinishEvent = detail::FinishEvent;
/// A write-only target: `nnh::Setter<Vec2>{[this, id](Vec2 v) { if (Enemy* e = find(id)) e->pos = v; }}`.
template <typename T>
using Setter = detail::Setter<T>;

/// The kinds of value a tween animates: float, Vec2 and Color (include <nnh/gfx.hpp> for Color).
template <typename T>
concept TweenValue = detail::is_tween_value<T>;

/// Options of one tween or timer, as designated initializers: `{.ease = nnh::Ease::BackOut, .loops = 2}`.
struct TweenOptions {
    Easing ease{};
    FrameTime delay{};                   ///< before the tween starts (and captures its start value)
    int loops = 1;                       ///< n ≥ 1, or kForever
    LoopMode loop_mode = LoopMode::Restart;
    TweenCallback on_finish{};           ///< after the final value is written, inside the set's update
    TweenCallback on_loop{};             ///< at the end of every loop but the last
    FinishEvent publish{};               ///< a game event queued on the bus when it finishes
};

/// Capacities of a set, required (no hidden default).
struct TweenSetConfig {
    int tweens = 0; ///< 1..kMaxTweens: tweens, timers (and, later, sequences and their steps) alive at once
    int steps = 0;  ///< 0..kMaxSteps sequence step records (0: no sequences)
};
inline constexpr int kMaxTweens = 1024;
inline constexpr int kMaxSteps = 4096;
inline constexpr int kMaxSequenceSteps = 64;
/// Callbacks that complete tweens whose callbacks complete tweens... stop calling back at this depth.
inline constexpr int kMaxCallbackDepth = 8;

/// A small copyable reference to a tween or timer of one set. The default handle is empty. A handle whose tween
/// ended (finished, killed, set cleared, slot reused) is stale: queries answer "not running", commands do nothing.
struct TweenHandle {
    std::uint16_t slot = 0;
    std::uint16_t set = 0;      ///< the set's serial; 0 = empty
    std::uint32_t generation = 0;

    constexpr bool operator==(const TweenHandle&) const = default;
};

/// Owns tweens and timers and plays them on the deterministic frame clock (FR-16): each update() is exactly one
/// 60 Hz frame (× speed), never measured wall time, so the same calls give the same values, callbacks and events,
/// bit for bit, on PC and 3DS. Keep the set in the object that owns the animated values (normally a scene).
///
/// Storage: one block reserved at creation (memory tag `tween`); nothing allocates afterwards. Move-only.
/// @code
/// nnh::TweenSet tweens_{{.tweens = 32}};
/// tweens_.to(&title_pos_, {200, 40}, nnh::Duration::from_ms(400), {.ease = nnh::Ease::BackOut});
/// tweens_.after(nnh::Duration::from_seconds(1), [this] { ready_ = true; });
/// tweens_.update(); // once per frame
/// @endcode
/// Misuse (a full set, an invalid time, a handle of another set, a null target) asserts in Debug/Development and
/// is ignored with an error log in Release (empty handle, no effect).
class TweenSet {
public:
    TweenSet() = default; ///< no storage: every start fails (asserts in Debug/Development)
    explicit TweenSet(const TweenSetConfig& config);
    ~TweenSet(); ///< kills everything silently (no callbacks, no events) and frees the block
    TweenSet(TweenSet&& other) noexcept;
    TweenSet& operator=(TweenSet&& other) noexcept;
    TweenSet(const TweenSet&) = delete;
    TweenSet& operator=(const TweenSet&) = delete;

    // Starting (FR-5, FR-6). Pointer targets: the value must outlive the tween (keep it in the set's owner).
    // `to` and `by` capture the start value from the target when the tween starts (after its delay); `from_to`
    // with no delay writes `from` at once. Setters have nothing to capture from, so they take from_to only.

    /// Animates `*target` to `end` over `time`.
    template <TweenValue T>
    TweenHandle to(T* target, std::type_identity_t<T> end, FrameTime time, const TweenOptions& options = {}) {
        return start({detail::tween_kind_of<T>, detail::TweenMode::To, target, nullptr, nullptr, &end}, time,
                     options);
    }
    /// Animates `*target` by `delta` (end = start + delta) over `time`.
    template <TweenValue T>
    TweenHandle by(T* target, std::type_identity_t<T> delta, FrameTime time, const TweenOptions& options = {}) {
        return start({detail::tween_kind_of<T>, detail::TweenMode::By, target, nullptr, nullptr, &delta}, time,
                     options);
    }
    /// Animates `*target` from `from` to `end` over `time`.
    template <TweenValue T>
    TweenHandle from_to(T* target, std::type_identity_t<T> from, std::type_identity_t<T> end, FrameTime time,
                        const TweenOptions& options = {}) {
        return start({detail::tween_kind_of<T>, detail::TweenMode::FromTo, target, nullptr, &from, &end}, time,
                     options);
    }
    /// Calls `target` with the value from `from` to `end` over `time` (once per update while it plays).
    template <TweenValue T>
    TweenHandle from_to(Setter<T> target, std::type_identity_t<T> from, std::type_identity_t<T> end, FrameTime time,
                        const TweenOptions& options = {}) {
        return start({detail::tween_kind_of<T>, detail::TweenMode::FromTo, nullptr, &target.fn_, &from, &end}, time,
                     options);
    }

    // Handles (FR-8). Stale or empty handles answer "not running" and are ignored.
    bool running(TweenHandle h) const;   ///< alive: waiting for its delay, playing or paused
    bool paused(TweenHandle h) const;
    float progress(TweenHandle h) const; ///< 0..1 of the current loop (0 during the delay or when stale)
    int loops_left(TweenHandle h) const; ///< including the current one; kForever when infinite; 0 when stale
    void pause(TweenHandle h);
    void resume(TweenHandle h);
    void kill(TweenHandle h);     ///< stops now; the value stays where it is; no callback, no event
    void complete(TweenHandle h); ///< jumps to the end value of the last loop, then runs its completion

    // Group commands (FR-9).
    void pause_all();
    void resume_all();
    void kill_all();
    void kill_target(const void* target); ///< kills every tween whose pointer target is `target`

    // Clock (FR-16, FR-17).
    void update();                ///< one 60 Hz frame × speed
    void advance(FrameTime time); ///< any span × speed (the sub-tick remainder carries to the next call)
    void set_speed(float speed);  ///< 0..64, rounded once to 1/4096 (0.5 is exact); 0 freezes the set
    float speed() const;

    int active() const;   ///< tweens and timers alive
    int capacity() const; ///< 0 without storage

private:
    TweenHandle start(const detail::TweenSpec& spec, FrameTime time, const TweenOptions& options);

    detail::TweenBlock* block_ = nullptr;
};

} // namespace nnh
