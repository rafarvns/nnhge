# Plan 015: Tweens and timers

Status: implemented (T7.2 passed on the Old 3DS XL, 2026-11-22) · approved 2026-11-22 (maintainer pre-approved)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D30 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 001 (event bus, scene stack), spec 002 (`Duration`, memory tags, asserts, logs, profiler), spec 013 (console
test scripts)

> **Amended by plan 016 (2026-11-22).** `Frames` and `FrameTime` (with `kMaxFrameTime`, the `Duration` conversion and
> the invalid marker, unchanged) are declared in the foundation header `<nnh/time.hpp>`, not in `<nnh/tween.hpp>`, so
> sprite animation ([plan 016](../016-sprite-animation/plan.md)) uses the frame clock without depending on tweens
> (constitution XI). `<nnh/tween.hpp>` includes `time.hpp`, so the API below and game code are unchanged. Whichever
> spec is implemented first adds them, with their conversion tests in `tests/time/test_frame_time.cpp` (T1.1's
> conversion table moves there; the tween clock cases stay in `tests/tween/test_frame_clock.cpp`). Likewise, the
> counting `operator new` of T4.4 lives in `tests/framework/alloc_counter.{hpp,cpp}`, shared by both modules (one
> definition per test binary).

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | Portable core module `<nnh/tween.hpp>`, no platform part, no contract change. Bit-identical results are a design goal (FR-3, FR-16), handled below in "Determinism" |
| II 3DS constraint | ✅ | Fixed capacities chosen by the game, one block per set (tag `tween`, ≈ 184 B per tween slot and 8 B per step on 3DS), nothing on the stack beyond a ~120 B options struct; 256 tweens < 1 ms per frame on an Old 3DS is checked (AC-7) |
| III Build speed | ⚠️→✅ | Pressure point: a header with templates. Answered by thin templates that only pack values and forward to non-template functions in `.cpp` files, a `detail` header for the adapters, and a forward-declared `Color`. Header cost measured before/after (T0.1, T8.3) |
| IV Event-driven | ✅ | Completion is a callback inside the owner's update or **the game's own** event on the bus (FR-11); progress and state are queries; the engine adds no event of its own |
| V No hidden costs | ✅ | No exceptions/RTTI/`virtual` per tween; callbacks stored inline (`SmallFunction`, the bus's 16-byte rule); no allocation after a set is created (AC-4) |
| VI Spec + test first | ✅ | Pure logic: easing, timer value, sets, sequences, the scene hook; all unit-tested on the fake platform first |
| VII Small files | ✅ | ≈ 17 new core files, each one concept, all planned ≤ 200 lines; one public header plus one detail header |
| VIII Documented API | ✅ | `docs/api/tween.md`, links from `scene.md` and `time.md`, example `examples/tweens` |
| IX Clean and legal | ✅ | Curves re-derived from Penner's published equations (BSD); own polynomial approximations; no code from easings.net (GPL) |
| X Safe by default | ✅ | Misuse asserts in Debug/Development and is ignored with a log in Release; stale handles are harmless (generation check); no external input is parsed |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure point: FR-15 needs a hook in the scene module. Answered by D30 (a generic, allocation-free attachment list: one pointer per scene, scenes know nothing about tweens). Easing and the timer value work alone; a game that uses none of it links no tween object file |

## Design

### Overview

```
game scene (member)                       engine
 nnh::TweenSet tweens_{*this, {.tweens = 32, .steps = 16}}
   │ one block (tag "tween"): header + slots[32] + steps[16]          scene stack (D30)
   │ attaches a SceneAttachment node (inside the block) ───────────▶  Scene::attachments_ (one pointer)
   │                                                                   ├ before top scene's update(): set update
   │                                                                   └ after a scene's on_exit(): set clear (silent)
 tweens_.to(&pos_, {200, 40}, Duration::from_ms(400), {.ease = Ease::BackOut})
   → TweenHandle {slot, generation, set serial}
 each update: +4096 ticks × speed  → for every active slot in start order:
   delay → capture start → p = elapsed / duration → e = ease(p) → value = from + (to − from)·e → write (pointer or setter)
   end of loop → on_loop / restart / yoyo (carry the leftover ticks) → end → write `to` exactly → on_finish, publish
```

### Decision D30: the scene attachment hook (FR-15)

The spec requires a bound set to update right before its scene's `update()`, only while the scene is on top, and to
be cleared silently after its scene leaves the stack. The hook must allocate nothing, cost unbound scenes at most a
pointer, and keep scenes ignorant of tweens.

**Chosen: scenes carry an intrusive list of attachments.**

- `Scene` gets one private member, `detail::SceneAttachment* attachments_ = nullptr` (forward-declared type; the
  public header gains no include). The `Scene` destructor's existing `NNH_VERIFY` also checks that the list is empty:
  a set must not outlive the scene it is bound to.
- `engine/core/src/scene/scene_attachments.hpp` (internal, the scene module's API for other modules) defines

  ```cpp
  namespace nnh::detail {
  enum class SceneHook : std::uint8_t { BeforeUpdate, AfterExit };
  struct SceneAttachment {
      SceneAttachment* next = nullptr;
      void (*notify)(SceneAttachment& self, SceneHook hook) = nullptr; // set by the owner
      std::uint32_t epoch = 0;                                          // pass in which it was attached
  };
  void scene_attach(Scene& scene, SceneAttachment& node);   // appended: hooks run in attach order
  void scene_detach(Scene& scene, SceneAttachment& node);   // safe during a hook pass
  void scene_notify(Scene& scene, SceneHook hook);          // used by SceneStack only
  }
  ```

- `SceneStack::update_and_draw()` calls `scene_notify(top, BeforeUpdate)` and then `top->update()`. `do_pop()` calls
  `on_exit()` and then `scene_notify(scene, AfterExit)`. `pop`, `replace`, the end of `run()` and `shutdown()` all
  go through `do_pop()`, so all four paths are covered. Clearing after `on_exit()` also discards anything that
  `on_exit()` itself started.
- Pass rules: a node attached during a pass waits for the next pass (its `epoch` is the current pass). Detaching
  during a pass is safe because the pass keeps a cursor, and `scene_detach` moves the cursor past the node it
  removes. Only one pass runs at a time (single thread, one top scene).
- `TweenSet` keeps its node and its `Scene*` inside its heap block, so the address is stable and the set itself can
  move. The node's `notify` is a static function in `tween/tween_scene.cpp`. The dependency points one way only:
  tween → scene internal header. The scene module never names the tween module.

Cost for scenes without sets: 4 B per `Scene` on 3DS (8 B on PC) and one null check per frame and per pop. The
`hello` executable gains the attach/notify code (target ≤ 256 B of text, measured in T5.4, AC-8); it links no tween
object.

Rejected alternatives:

- **The game calls `tweens_.update()` in `update()` and `clear()` in `on_exit()`.** This is the error-prone duty the
  spec removes. Forgetting the clear is the "callback after the scene is gone" bug.
- **A `virtual` hook on `Scene`** (`virtual TweenSet* tweens()`). Scenes would depend on tweens, and only one set per
  scene would be possible.
- **A global registry `Scene* → sets`.** It needs a capacity and a lookup per frame, and it is hidden global state.
- **A `TweenScene` base class.** Inheritance forces a structure on the game (XI), and a scene that already derives
  from something else couldn't use it.
- **Scene events (`SceneExited`).** They are delivered at the next dispatch, which is too late (the set would play one
  more frame into a scene that is gone), and "before update" can't be expressed with them.

The same attachment can later serve other scene-bound helpers (sprite animators, scene-local timers in UI) without
changing `Scene` again.

### Decision on `Color` (spec question): it stays in `<nnh/gfx.hpp>`

`<nnh/tween.hpp>` **forward-declares** `struct Color;`, exactly as plan 014 does for data assets. The templates only
use `Color` when instantiated. A game that animates a colour already includes `<nnh/gfx.hpp>` to have one. The
packing and writing of `Color` live in `tween/tween_values.cpp`, which includes `gfx.hpp`. Moving `Color` to a small
`<nnh/color.hpp>` (included by `gfx.hpp`, so no game code changes) remains open for the first module that needs the
complete type in its own header. Both 014 and 015 stay compatible with that move. T0.1 measures the header cost.

### Public API (sketch)

```cpp
// <nnh/tween.hpp>: includes <cstdint>, "nnh/time.hpp", "nnh/math.hpp", "nnh/detail/tween_detail.hpp"
// (which includes small_function.hpp and type_id.hpp, already used by events.hpp). Forward-declares Color, Scene.
namespace nnh {

// ---- Easing (FR-1..FR-3): usable alone, no state ----
enum class Ease : std::uint8_t {
    Linear,
    QuadIn, QuadOut, QuadInOut, CubicIn, CubicOut, CubicInOut, SineIn, SineOut, SineInOut,
    BackIn, BackOut, BackInOut, ElasticIn, ElasticOut, ElasticInOut, BounceIn, BounceOut, BounceInOut,
};
inline constexpr int kEaseCount = 19;
using EaseFn = float (*)(float t);
/// Exactly 0 at t <= 0 (and NaN), exactly 1 at t >= 1; same bits on every system (docs: error bounds).
float ease(Ease curve, float t);
/// A built-in curve or the game's own function, accepted wherever a curve is.
struct Easing {
    constexpr Easing(Ease e = Ease::Linear) : builtin(e) {}
    constexpr Easing(EaseFn f) : custom(f) {}
    float operator()(float t) const;           // custom ? custom(t) : ease(builtin, t)
    Ease builtin = Ease::Linear;
    EaseFn custom = nullptr;
};

// ---- The frame clock (FR-16): declared in <nnh/time.hpp> (amended by plan 016), shown here for reference ----
struct Frames { std::int32_t count = 0; };   ///< a whole number of 60 Hz frames
/// A span of the frame clock in ticks of 1/4096 frame. Implicit from Duration (rounded to the nearest tick,
/// 1 s = 60 frames exactly) and from Frames. Range 0..kMaxFrameTime (1 hour); outside it, ticks = -1 (invalid:
/// asserts where used).
struct FrameTime {
    static constexpr std::int32_t kTicksPerFrame = 4096;
    constexpr FrameTime() = default;
    constexpr FrameTime(Duration d);   // (ns·6144 + 12 500 000) / 25 000 000, ns in [0, 3600 s]
    constexpr FrameTime(Frames f);     // count · 4096, count in [0, 216 000]
    constexpr Duration duration() const;   // rounded to the nearest ns
    constexpr float frames() const;
    std::int32_t ticks = 0;
};

// ---- The timer value (FR-14): plain struct, no set, no init ----
class Timer {
public:
    Timer() = default;                           ///< stopped
    static Timer once(FrameTime time);           ///< running, fires once
    static Timer repeating(FrameTime period);    ///< running, fires every period
    int update();                                ///< one frame; returns how many times it fired (0, 1, more)
    int advance(FrameTime time);                 ///< any span (FixedStep, skips)
    bool running() const; bool paused() const;
    FrameTime elapsed() const; FrameTime remaining() const; float progress() const;
    void pause(); void resume(); void stop(); void restart();
private:
    std::int32_t period_ = 0, elapsed_ = 0; std::uint8_t flags_ = 0;
};

// ---- Sets (FR-4..FR-13, FR-15..FR-18) ----
enum class LoopMode : std::uint8_t { Restart, Yoyo };
inline constexpr int kForever = -1;
using TweenCallback = detail::SmallFunction<void()>;           // ≤ 16 bytes of captures
/// A copy of one of the game's events, published on finish: `.publish = IntroFinished{}`.
using FinishEvent = detail::FinishEvent;                       // trivially copyable, ≤ 32 B, align ≤ 8
template <typename T> using Setter = detail::Setter<T>;        // nnh::Setter<Vec2>{[this, id](Vec2 v) {...}}

struct TweenOptions {
    Easing ease{};
    FrameTime delay{};
    int loops = 1;                       ///< n ≥ 1 or kForever
    LoopMode loop_mode = LoopMode::Restart;
    TweenCallback on_finish{};
    TweenCallback on_loop{};
    FinishEvent publish{};
};
struct SequenceOptions { FrameTime delay{}; int loops = 1; LoopMode loop_mode = LoopMode::Restart;
                         TweenCallback on_finish{}; TweenCallback on_loop{}; FinishEvent publish{}; };
struct TweenSetConfig {
    int tweens = 0;                      ///< required: 1..kMaxTweens (tweens, timers, sequences and their steps)
    int steps = 0;                       ///< 0..kMaxSteps sequence step records (0: no sequences)
};
inline constexpr int kMaxTweens = 1024, kMaxSteps = 4096, kMaxSequenceSteps = 64, kMaxCallbackDepth = 8;

/// Small copyable handle. Default: empty (stale).
struct TweenHandle { std::uint16_t slot = 0; std::uint16_t set = 0; std::uint32_t generation = 0; };

template <typename T> concept TweenValue = detail::is_tween_value<T>;   // float, Vec2, Color

class SequenceBuilder;    // below

class TweenSet {
public:
    TweenSet() = default;                                   ///< no storage: every start fails (logs)
    explicit TweenSet(const TweenSetConfig& config);        ///< unbound: the game calls update()
    TweenSet(Scene& scene, const TweenSetConfig& config);   ///< bound: updated/cleared with `scene` (D30)
    ~TweenSet();                                            ///< kills everything silently, frees the block
    TweenSet(TweenSet&&) noexcept; TweenSet& operator=(TweenSet&&) noexcept;   // move-only

    // Start (FR-5, FR-6). Pointer targets: the value must outlive the tween. Setters have no read side, so
    // they take an explicit start: from_to only.
    template <TweenValue T> TweenHandle to(T* target, T end, FrameTime time, const TweenOptions& o = {});
    template <TweenValue T> TweenHandle by(T* target, T delta, FrameTime time, const TweenOptions& o = {});
    template <TweenValue T> TweenHandle from_to(T* target, T from, T end, FrameTime time, const TweenOptions& o = {});
    template <TweenValue T> TweenHandle from_to(Setter<T> target, T from, T end, FrameTime time,
                                                const TweenOptions& o = {});
    // Timers (FR-13)
    TweenHandle after(FrameTime time, TweenCallback fn);
    TweenHandle after(FrameTime time, FinishEvent event);
    TweenHandle every(FrameTime period, TweenCallback fn, int times = kForever);
    // Sequences (FR-10)
    SequenceBuilder sequence(const SequenceOptions& options = {});

    // Handles (FR-8): stale handles answer "not running" and are ignored; a handle of another set asserts.
    bool running(TweenHandle h) const;  bool paused(TweenHandle h) const;
    float progress(TweenHandle h) const; int loops_left(TweenHandle h) const;   // kForever when infinite
    void pause(TweenHandle h); void resume(TweenHandle h); void kill(TweenHandle h); void complete(TweenHandle h);
    // Group commands (FR-9)
    void pause_all(); void resume_all(); void kill_all(); void kill_target(const void* target);

    // Clock (FR-16, FR-17). Unbound sets only (asserts on a bound set).
    void update();                       ///< one frame: + 4096 ticks × speed
    void advance(FrameTime time);        ///< any span, × speed
    void set_speed(float speed);         ///< 0..64, rounded once to 1/4096 (0.5 exact), then exact integer math
    float speed() const;

    int active() const; int capacity() const; bool bound() const;
private:
    detail::TweenBlock* block_ = nullptr;
};

/// Built by TweenSet::sequence(); appends steps in place (no allocation). Converts to the sequence's handle.
class SequenceBuilder {
public:
    template <TweenValue T> SequenceBuilder& to(T* target, T end, FrameTime time, const TweenOptions& o = {});
    // ... by, from_to (pointer and setter), like TweenSet
    SequenceBuilder& join();              ///< the next step plays together with the previous one
    SequenceBuilder& wait(FrameTime time);
    SequenceBuilder& call(TweenCallback fn);
    TweenHandle handle() const;
    operator TweenHandle() const { return handle(); }
};
} // namespace nnh
```

Notes on the API:

- **Names.** The spec writes easings as `back_out`. The plan uses the project's enumerator style
  (`Ease::BackOut`, as in `Button::Start`). The set is `TweenSet`, the handle is `TweenHandle`, and the clock type is
  `FrameTime`.
- **Options, not builders, for single tweens.** Designated initializers match `nnh::init({...})`, and every option is
  known when the tween is created. FR-6's "write `from` at creation when there is no delay" needs that, and a chained
  `.delay()` after `.from()` would make it ambiguous.
- **Templates are thin.** Each template packs the value into `float[4]` (Vec2 uses 2 floats, Color 4 channels, float
  1), builds a `detail::TweenSpec` (kind, target, setter, from/end, flags) and calls one non-template function
  (`detail::tween_start(block_, spec, time, options)`). Game code instantiates only a few lines per type.
- **Setters**: `detail::Setter<T>` wraps a `SmallFunction<void(const void*)>` adapter (like `EventAdapter`), so a lambda
  `void(T)` with at most 16 bytes of captures is stored inline. A setter target supports `from_to` only, because
  there is nothing to capture from. This is FR-6's rule applied to a write-only target, and the docs say so.
- **`FinishEvent`**: `detail::TypeId` + size + a 32-byte aligned buffer. Its template constructor checks
  `is_trivially_copyable`, `sizeof ≤ 32` and `alignof ≤ 8` with `static_assert`s. On finish, the set calls the existing
  `detail::events_publish(type, bytes, size)`, so delivery follows the bus's rules.
- **Sets before `nnh::init()`**: storage comes from `mem::alloc`, which needs no init. Only publishing on finish needs
  the bus, and it asserts there as `events::publish` does.

### Events introduced

None. Completion publishes **the game's own** event (FR-11). There is no engine `TweenFinished` (one event per
meaning, [events.md](../../docs/architecture/events.md)).

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| (game-defined, e.g. `IntroFinished{}`) | a `TweenSet` when a tween/sequence/timer with `.publish` finishes | the game's struct, ≤ 32 B | other scenes, the HUD, the audio code |

### Semantics fixed by the plan

- **Clock.** Times are `int32` ticks of 1/4096 frame. `update()` adds `4096 × speed` ticks. A tween with duration `D`
  ticks ends on the first update where `elapsed ≥ D`. Leftover `elapsed − D` carries into the next loop and the next
  sequence step, so nothing drifts. Durations that are whole frames end exactly on their update number: 0.5 s = 30
  updates, 1 s = 60, `Frames{n}` = n. `Duration` → ticks rounds half up (16.67 ms → 4096 ticks exactly).
  `kMaxFrameTime` = 1 hour (884 736 000 ticks < 2³¹). Speed is stored as ticks per update (0.5 → 2048, exact).
  `advance(t)` scales by speed with an integer remainder kept in the set, so there is no accumulated error. Zero
  durations finish on their first update. Infinite loops of zero length assert.
- **Start of counting.** A tween started outside an update counts on the next update, so its first update is frame 1.
  A tween started *during* an update (from a callback) is marked `fresh` and is skipped by that update (FR-12).
  `from_to` with no delay writes `from` at once, in both cases. For the first group of a sequence with no delay,
  `from` is written when the step is appended.
- **Start values.** `to`/`by` capture the target when the tween really starts (end of its delay, or when its sequence
  first reaches it). `by` sets `end = start + delta` then. The capture happens once: later loops reuse it, so
  `Restart` jumps back to the captured start.
- **Values.** Progress is `p = float(elapsed) / float(D)`, then `e = ease(p)`. The value is `from + (to − from)·e` per
  component. Color channels are computed as floats, clamped to 0–255 and rounded with `int(v + 0.5f)`. When a tween
  finishes, it writes `to` **exactly**, not the interpolated value at `e = 1`. Yoyo backwards evaluates the forward
  value at the mirrored time `D − τ`, which is the mirrored easing.
- **Order.** Active slots form a doubly linked list in start order, separate from slot order. Kills during an update
  only mark the slot dead (the generation is bumped at once, so handles go stale at once). Dead slots are unlinked
  and freed when the update ends, so iteration never follows a freed link, and a killed tween is never updated or
  called again. New tweens take free slots and are appended to the list's tail.
- **Completion.** For a finished tween, the order is: write the final value, then `on_finish`, then queue the event.
  `kill`, `kill_all`, `kill_target`, set destruction and the scene's automatic clear run no callbacks and publish
  nothing. `complete(h)` writes the end of the last loop (for an even yoyo count, the start value; for infinite
  tweens, the end of the current loop), then runs `on_finish` and the event. It does not run `on_loop` or
  intermediate sequence calls, and a tween still in its delay first captures its start. `complete` called from a
  callback nests. At depth `kMaxCallbackDepth` (8), further completions still jump to the end but skip their
  callbacks and events, with one `NNH_LOG_ERROR`.
- **Sequences.** A sequence is a slot of kind `Sequence` plus a linked list of step records. Each step references a
  child slot: tween, interval (a timer with no callback) or call (a zero-length timer with a callback). `join()`
  merges the next step into the current **group**. A group's length is the longest member (member delay + duration ×
  loops). Children may not loop forever (that asserts). Groups run one after another with carry, and zero-length
  groups (calls) run within the same update. Yoyo backwards walks the groups in reverse and evaluates each child at
  its mirrored local time. Call steps run each time the sequence passes them, in either direction (as in DOTween).
  Progress is measured against the total length, computed when the sequence is built. A child's `on_finish`/event
  fires when that child ends.
- **`kill_target(p)`** kills standalone tweens whose pointer target is `p`, and **whole sequences** containing such a
  tween, so that half a sequence never keeps running on an object that is going away.
- **Timers.** `after(t, fn)` is a timer slot with one period. `every(period, fn, n)` fires `fn` each period (n times or
  forever), and every crossing fires, even several in one `advance` (FR-14 has the same rule). A period must be ≥ 1
  tick (asserts).

### Determinism (FR-3, FR-16)

- **The curves call no libm.** `quad`, `cubic`, `back` and `bounce` are polynomials and comparisons. `sine` uses
  `sin_quarter(u)` = sin(π/2·u) for u ∈ [0, 1], an odd degree-7 minimax polynomial (error < 2·10⁻⁶), with
  cos(π/2·u) = sin_quarter(1 − u). `elastic` uses Penner's form with `exp2_neg(x)` (2^x for x ∈ [−10, 0]: the integer
  part builds the exponent bits with `std::bit_cast`, the fraction uses a degree-5 polynomial, relative error < 10⁻⁶)
  times `sin_turns(x)`. `sin_turns` works in periods: the argument is reduced by the integer part of `x`, a quarter
  is selected, then `sin_quarter` is applied. The docs state the bounds, which are far below FR-3's 1/4096. Each easing
  returns exactly 0 for `t ≤ 0` or NaN and exactly 1 for `t ≥ 1`.
- **No subnormals.** For `0 < t < 2⁻²⁴`, every curve returns 0. As a result, no intermediate result is subnormal
  (`t³ ≥ 2⁻⁷²`).
- **FP environment, checked in the installed toolchain.** libctru writes FPSCR = `0x03000000` (round to nearest,
  flush-to-zero, default NaN) in `__system_initSyscalls` and for every thread in `initThreadVars` (disassembly of
  `libctru.a`). The 3DS kernel's own default, round toward zero, is therefore overridden, and the 3DS rounds like an
  x86-64 PC with SSE. The one remaining difference is flush-to-zero on the 3DS against gradual underflow on the PC.
  The curves avoid subnormals (above). The docs state that values tweened to subnormal magnitudes (|x| < 2⁻¹²⁶) are
  not guaranteed bit-identical. The example's self-check also logs a rounding probe (`1 + 1.5·2⁻²⁴` must round up),
  so a future toolchain change shows up as a FAIL.
- **Compiler flags.** The project builds with `-std=c++20` (extensions off), no `-ffast-math` and no `-march` on the
  PC. devkitARM uses `-march=armv6k -mfloat-abi=hard` (VFPv2: `vmla` is chained, not fused). Today no target can
  fuse a multiply-add. GCC's C++ default is still `-ffp-contract=fast`, though, and future AArch64 targets (Switch) or
  hosts built with FMA would fuse. So the tween module's sources get `-ffp-contract=off`
  (`set_source_files_properties`, GCC/Clang only). Every float operation of the module lives in those `.cpp` files
  (`ease()` is not inline), so the flags apply to all of it. T0.2 checks for fused instructions with `objdump` on
  every compiler. Float↔int conversions and IEEE `+ − × ÷` in round-to-nearest are exact operations on every target.
- **Golden values.** `tests/tween/test_ease_golden.cpp` pins the bit patterns of all 19 curves at 33 points. Those
  ctests run with WinLibs GCC, GCC 13/14 and clang 18 (VM 120). The 3DS compares the same values through the
  example's self-check checksum (AC-5).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/tween.hpp` | new | public API above (≈ 200 lines with comments; the one public header of the module) |
| `engine/core/include/nnh/detail/tween_detail.hpp` | new | `is_tween_value`, packing templates, `Setter<T>`, `FinishEvent`, `TweenSpec`, forward declarations of `TweenBlock` and the non-template entry points |
| `engine/core/include/nnh/scene.hpp` | changed | `detail::SceneAttachment* attachments_` member, destructor check, friend for `scene_attachments.cpp` |
| `engine/core/src/scene/scene_attachments.hpp` / `.cpp` | new | D30: node type, attach/detach, notify pass with epoch and cursor |
| `engine/core/src/scene/scene_stack.cpp` | changed | notify `BeforeUpdate` before the top `update()`, `AfterExit` after `on_exit()` |
| `engine/core/src/tween/ease_math.hpp` / `.cpp` | new | `sin_quarter`, `sin_turns`, `exp2_neg` approximations |
| `engine/core/src/tween/ease.cpp` | new | the 19 curves, `ease()`, `Easing::operator()` |
| `engine/core/src/tween/timer.cpp` | new | `Timer` value type |
| `engine/core/src/tween/tween_block.hpp` | new | `TweenBlock` header, `SlotHot`, `SlotCold`, `Step`, states and flags, `static_assert` size bounds |
| `engine/core/src/tween/tween_internal.hpp` | new | functions shared by the module's files (slot lookup, handle check, finish, write) |
| `engine/core/src/tween/tween_set.cpp` | new | construction/destruction (one `mem::alloc`, tag `tween`), move, serials, capacity, log channel `tween`, handle validation |
| `engine/core/src/tween/tween_start.cpp` | new | `tween_start`, `after`/`every`, `from` written at creation, full-set errors |
| `engine/core/src/tween/tween_values.cpp` | new | pack/unpack, interpolate float/Vec2/Color, write by pointer or setter, capture from pointer |
| `engine/core/src/tween/tween_advance.cpp` | new | one slot's advance: delay, capture, loops, yoyo, carry, timer fires |
| `engine/core/src/tween/tween_update.cpp` | new | `update`/`advance`/speed, start-order list, fresh flag, sweep of dead slots, profile scope |
| `engine/core/src/tween/tween_commands.cpp` | new | queries, pause/resume/kill/complete, group commands, `kill_target` |
| `engine/core/src/tween/tween_finish.cpp` | new | completion: final write, callbacks, event publish, depth limit |
| `engine/core/src/tween/sequence_build.cpp` | new | `SequenceBuilder`: step records, join, wait, call, limits, failure handling |
| `engine/core/src/tween/sequence_advance.cpp` | new | groups, carry, zero-length groups, yoyo backwards, total length |
| `engine/core/src/tween/tween_scene.cpp` | new | binding: attach/detach, notify → update or silent clear, "bound set updated by hand" check |
| `engine/core/CMakeLists.txt` | changed | explicit source list, `-ffp-contract=off` on `src/tween/*` sources (GCC/Clang) |
| `tests/tween/test_*.cpp` | new | see Test strategy; `tests/tween/alloc_counter.cpp` (counting global `operator new`/`delete`), `tween_fixture.hpp` |
| `tests/scene/test_scene_attachments.cpp` | new | D30 with a fake attachment |
| `tests/tweens/test_self_check.cpp` | new | the example's self-check script, run on the fake platform against the golden checksum |
| `tests/CMakeLists.txt` | changed | the new test files |
| `examples/tweens/` | new | `main.cpp`, `showcase_scene.{hpp,cpp}` (top: gallery; bottom: intro and controls), `easing_gallery.{hpp,cpp}`, `intro.{hpp,cpp}`, `overlay_scene.{hpp,cpp}` (pushed scene with its own bound set), `self_check.{hpp,cpp}` (pure script + checksum), `self_check_scene.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(tweens)` |
| docs (stage 8) | changed/new | `docs/api/tween.md` (+ curve picture), `docs/api/scene.md`, `docs/api/time.md`, `docs/architecture/scenes.md` (attachments, D30), `docs/architecture/README.md` if it lists modules, `docs/REFERENCES.md`, `CLAUDE.md` layout, `docs/ROADMAP.md`, `specs/README.md` |

### Data and memory

One block per set, from `mem::alloc(MemRegion::Heap, tween, …)`, laid out as `[TweenBlock][SlotHot × n][SlotCold ×
n][Step × s]`. The hot part is what `update()` touches every frame. The cold part holds the callbacks and the event,
read only on loop or finish.

| Record | 3DS (32-bit, `SmallFunction` = 24 B) | PC x64 (`SmallFunction` = 32 B) |
|--------|-------------------------------------|---------------------------------|
| `TweenBlock` header (capacities, serial, speed, remainder, free list, list head/tail, depth, flags, `SceneAttachment`, `Scene*`) | ≈ 64 B | ≈ 96 B |
| `SlotHot`: generation 4, state/kind/flags/ease 4, list links 4, parent/step 4, delay/duration/elapsed/loops 16, from/to 32, pointer target, setter, custom easing | ≈ 96 B | ≈ 112 B |
| `SlotCold`: `on_finish`, `on_loop`, `FinishEvent` (type + size + 32 B) | ≈ 88 B | ≈ 112 B |
| `Step`: child slot, next, flags (join, kind) | 8 B | 8 B |

`static_assert(sizeof(SlotHot) + sizeof(SlotCold) <= (sizeof(void*) == 4 ? 192 : 256))` runs in every build, so the
3DS bound is checked by the console build itself. Examples on 3DS: the example's showcase set {32, 16} ≈ 6 KB; the
bench's set {256, 0} ≈ 47 KB; the ceiling {1024, 4096} ≈ 220 KB. Each step in a sequence uses one tween slot and one
step record (documented). `Timer` is 12 B. `TweenSet` itself is one pointer, so scene objects stay small and a scene
kept as a local in `main` costs the 3DS main-thread stack nothing (spec constraint II).

Sets that outlive `nnh::shutdown()` show up in the leak report as tag `tween`, like textures. The docs and the
example destroy scenes and sets before shutdown, as `examples/platformer` does.

### Platform work

- **pc**: none. Only the compile flag on the module's sources.
- **3ds**: none. The determinism premise (libctru's FPSCR) is verified, and the console run of the self-check
  confirms it (AC-5).
- **fake (tests)**: none new. Tests use the fake platform's `nnh::init`/`frame()` for event delivery and scenes.

### Performance (AC-7)

Per active tween per update: one list step, an integer add/compare, one float divide, one curve (the worst is elastic:
`exp2_neg` + `sin_turns`, ≈ 40 VFP operations), 1–4 lerps and a write, touching ≈ 96 B. On the Old 3DS ARM11
(268 MHz, VFPv2) that is an estimated 300–500 cycles, so 256 tweens ≈ 0.3–0.5 ms, under the 1 ms target. The bench
mode measures it with `time::now()` around `update()`, averaged over 600 frames and with the worst frame reported,
in the flavor that `nnh run` builds (Development). If the target is missed, the order of fixes is: cache `1/D` per
loop (one multiply instead of a divide), a branch-free curve dispatch, then reporting the real number and adjusting
the docs' target with the maintainer.

#### Stage 0 results (2026-11-22)

Baseline before any tween code (T0.1, Windows, WinLibs GCC 16.2.0 and devkitARM GCC 16.1.0; three runs, the first
one after a header change, so it rebuilds more; another agent was building in the main tree at the same time, so the
times are noisy):

| Measure | Value |
|---------|-------|
| `hello.exe` pc-debug (`size`: text / data / bss) | 196 572 / 26 756 / 210 128 |
| `hello.exe` pc-release | 129 080 / 21 988 / 207 536 |
| `hello.elf` 3ds-release · `hello.3dsx` | 216 084 / 28 328 / 102 576 · 261 460 B |
| `hello.elf` 3ds-dev · `hello.3dsx` | 232 464 / 32 848 / 105 144 · 282 564 B |
| edit `examples/hello/main.cpp` → simulator (pc-debug) | 5.92 / 1.47 / 1.61 s |
| edit `examples/hello/main.cpp` → `.3dsx` (3ds-release) | 5.75 / 2.59 / 2.92 s |
| clean `nnh_core` (pc-debug, Ninja, 24 threads) | 3.22 / 3.36 / 3.19 s |
| one-line TU `#include <nnh/events.hpp>`: preprocessed lines · `-fsyntax-only` | 7 633 · 0.45 / 0.39 / 0.38 s |

Determinism probe (T0.2, scratch `probe.cpp` with draft `sin_quarter`, `exp2_neg`, `elastic_out`):

- **WinLibs GCC 16.2.0 (x86-64)**: the C++ default is `-ffp-contract=fast` (`g++ -Q --help=optimizers`). Without
  `-march` there is no FMA instruction to fuse into: `objdump -d` shows no `vfmadd`/`vfmsub`/`vfnm` with or without
  `-ffp-contract=off`, and the printed bits (checksum `c1cd8013`) are identical at `-O0`/`-O2`, with and without the
  flag. The golden table of T1.4 was also generated at `-O0` and `-O2`, with and without the flag: identical.
- **devkitARM GCC 16.1.0** (`-march=armv6k -mfloat-abi=hard`): `vmla.f32`/`vnmls.f32` appear **in both modes**. They
  are VFPv2's chained multiply-accumulate (the product is rounded first), so they give the same bits as a separate
  multiply and add; GCC does not treat them as contraction. No `vfma` (VFPv2 has none).
- **libctru**: `objdump -d libctru.a` shows `mov r1, #0x3000000; vmsr fpscr, r1` in both `__system_initSyscalls` and
  `initThreadVars`: round to nearest, flush-to-zero, default NaN, as the plan states.
- **VM 120 (2026-11-22):** ctest with GCC 13/14 and clang 18 green; the golden bit patterns and the self-check
  checksum match WinLibs GCC. The 3DS bits come with the example's self-check (T7.2).

#### Stage 1–2 refinements (2026-11-22, implemented)

- `FrameTime` also has `kInvalid` (-1), `kMaxTicks`, `kMaxFrames`, `from_ticks()`, `valid()` and `operator==`;
  `duration()` of the invalid marker is `Duration{-1}`. Its tests are `tests/time/test_frame_time.cpp` (amendment by
  plan 016); 100 ms = 24 576 ticks is in the table for plan 016.
- The log channel `tween` lives alone in `tween/tween_log.cpp`, not in `tween_set.cpp`: `nnh::Timer` logs misuse
  through it, and a game that only uses `Timer` must not link the set code (FR-19). The memory tag stays in
  `tween_set.cpp` and is reached through `detail::tween_memory_tag()` (as `audio_memory_tag()`).
- `Timer`: `running()` stays true while paused; `stop()` halts **and rewinds** (remaining = the whole period);
  `restart()` clears a pause; a fired zero-length one-shot reports progress 1. Invalid times and a repeating
  period of 0 assert, and in Release leave the timer stopped.
- Value parameters of `to`/`by`/`from_to` are `std::type_identity_t<T>`, so `to(&x, 10, ...)` deduces `T` from the
  target only (an `int` literal works). The setter form needs an explicit `nnh::Setter<T>{...}`, as the plan wrote.
- "Fresh" tweens need no flag: a pass stops at the list tail it saw when it began, and new tweens are appended after
  it, so they start counting at the next update (FR-12 unchanged).
- Kills only mark slots while the block is **busy** — inside an update *or* inside `complete()` — and the outermost
  leave sweeps them. So a callback run by `complete()` outside an update can't see the slot it runs from reused.
  A set destroyed or move-assigned while busy asserts.
- A default-constructed set (no storage) treats every start as "full" (error log + assert).
- One `advance()` call covers at most 1 hour of set time after the speed is applied (the ticks are clamped).
- Stale handles: `progress()` and `loops_left()` answer 0.
- Measured record sizes: 3DS `SlotHot` 96 B + `SlotCold` 88 B = 184 B (as planned), `TweenBlock` 48 B,
  `TweenOptions` 112 B, `Timer` 12 B; PC x64 112 + 112 = 224 B, `TweenBlock` 64 B, `TweenOptions` 144 B.
- The test framework's `kMaxTests` went from 1024 to 2048 (the suite passed 1024 tests).

#### Stage 3 refinements (2026-11-22, implemented)

- `after(time, fn)` and `after(time, Event{})` were ambiguous: both `TweenCallback` (`SmallFunction`) and
  `FinishEvent` had unconstrained converting constructors. Now `SmallFunction`'s constructor accepts only callables
  (`std::is_invocable_v<F&, Args...>`; every valid event handler still converts, the bus is unchanged) and
  `FinishEvent`'s only non-callables. No new overloads or templates in `TweenSet`.
- `every(period, fn, times)` stores `fn` as both `on_loop` and `on_finish`: each period but the last calls `on_loop`,
  the last calls `on_finish`, so `fn` runs every period, and `complete()` calls it once.
- `on_loop` runs at the end of every loop but the last (the last runs `on_finish`). If it pauses its own tween, the
  rest of that update's time stays in the loop (the value is written, no further loops end in that update).
- The depth limit counts callbacks (`on_finish` and `on_loop`) on the stack: at depth 8 a completion still writes
  its end value and ends, but skips its callbacks and event, with one error per outermost update or `complete()`.
- A set destroyed or move-assigned from its own callback asserts in Debug/Development. In Release it is logged,
  everything in it is killed (nothing else is advanced or called back) and the block is freed when the outermost
  update or `complete()` returns: no use after free (`TweenBlock::doomed`, test in `test_tween_misuse.cpp`).
- Publishing on finish before `nnh::init()` relies on the bus's own assert; Release drops it like
  `events::publish` does. The tween itself still finishes.
- The re-entrancy tests (T3.3) passed on their first run: the stage 2 design (a pass stops at the tail it saw; kills
  only mark slots while busy) already gave FR-12's rules. They stay as regression tests, and mutation testing
  (below) checks that they bite.
- Tests that assert inside an update use `tween_test::recover_after_crash(set)` (test-only: marks the block idle
  after the crash path jumped out of the update).
- **Hand mutation testing** (stages 1–3, `nnh_tests` in pc-debug; scratch runner, files restored and touched after
  each mutant): 55 mutants over the curves and approximations, `FrameTime`, `Timer`, the update loop and clock,
  slot advance, values, handles and commands, starts and completion. 45 were killed at once. 6 real survivors got
  new tests, which kill them: the sine polynomial rounds above 1 for 25 floats in [0.99983, 1) (an exhaustive
  search of [0, 1]; the clamp is needed: a bit-by-bit test of [0.996, 1)); the speed is rounded to nearest, not
  truncated; a frozen set (speed 0) starts and finishes nothing; a tween starts on the very update its delay ends;
  an `on_loop` that pauses its tween stops further loops in that update; `kill_target(nullptr)` spares timers and
  setters. 4 were equivalent: three were dead code, now removed (a clamp in `bounce_out`, which never exceeds 1 for
  any float of [0, 1]; a "started" check in `progress()`, where `elapsed` is 0 until the start; a "doomed" check
  before callbacks), and one swaps the final write with the kill inside `tween_finish` (not observable). Result:
  51/51 non-equivalent mutants killed.

#### Stage 4 refinements (2026-11-22, implemented)

- **Sequences are played by position, not by advancing children.** Each update moves the sequence's position along
  its loop (integer ticks; backwards in a yoyo loop) and plays every step the move crosses. A step is evaluated at
  its local time (its own delay, loops and yoyo included), so carry across steps and loops, the mirrored yoyo and
  `complete()` need no per-child state. The children are slots off the start-order list, reached through the step
  records; `active()` counts tweens, timers and sequences, not a sequence's steps. A sequence is one slot plus one
  slot and one step record per step, as planned.
- **Crossing rules.** A forward move plays what lies in (p0, p1]; at the start of the first loop and of every
  restart p0 is -1, so steps at 0 play. A backward move plays [p1, p0), so a call step at a turning point never runs
  twice in a row (forward then straight back). A step's start value is captured when the move first reaches it (the
  end of its own delay). Within one update the crossed steps play group by group in step order (forwards) or in
  reverse (backwards), not strictly sorted by time inside a group.
- **Callbacks.** Call steps run in both directions. A child's `on_finish`, `on_loop` and event run only in forward
  play, when that child ends (or loops). If a callback ends the sequence (kill, complete, kill_target) the update
  stops at once; if it pauses the sequence, the rest of that update's move still plays (values stay consistent) and
  no further loop starts.
- **Builder rules.** A failed append (full set, out of step records, more than 64 steps, a step looping forever, an
  invalid step, a sequence longer than 1 hour, or steps appended after the sequence started) removes the whole
  sequence and leaves an empty handle (asserts in Debug/Development). `from` of the first group is written at append
  when neither the sequence nor the step has a delay. An infinite sequence of length 0 (only calls) asserts at its
  first update and is removed in Release.
- `SequenceBuilder` also has `by()` (the plan listed it implicitly); `SequenceOptions` mirrors `TweenOptions`.
  `SlotHot` gained `first_step` inside existing padding (sizes unchanged: 3DS 96 + 88 B, PC 112 + 112 B);
  `TweenBlock` is now 68 B on 3DS and 96 B on PC (step free list, scene attachment, scene pointer).
- The "three 0.5 s steps end on update 90" case lives in `tests/tween/test_sequence.cpp` (with the other sequence
  playing tests) instead of `test_frame_clock.cpp`.
- **Alloc counter** (`tests/framework/alloc_counter.{hpp,cpp}`, in `nnh_test_framework`): a counting replacement of
  the global scalar `operator new`/`delete` (libstdc++'s array and nothrow forms forward to it), with
  `enable(bool)`, `reset()`, `count()`. Its own test keeps the pointer in a `volatile` so Release can't elide the
  new/delete pair.

#### Stage 5 refinements and results (2026-11-22, implemented)

- D30 as planned, with two refinements for size: the pass state and `scene_notify()` live in
  `scene/scene_notify.cpp`, and `scene_attach`/`scene_detach` in `scene/scene_attachments.cpp`, so a game with
  scenes but no bound set links only `scene_notify()`. `SceneAccess` (the friend of `Scene`) is defined in the
  internal header. The `Scene` destructor keeps **one** `NNH_VERIFY` (its message now also names a TweenSet still
  attached) instead of two, because that code is inlined into every game scene's destructor. Notifying from inside a
  notification is an `NNH_ASSERT` (it cannot happen through the scene stack, whose changes are deferred).
- A bound set ignores a notification while it is busy (also impossible through the stack), detaches at once when it
  is destroyed or move-assigned (also on the misuse path that defers freeing), and `bound()` reports it.
- **T5.4 opt-in check** (`hello`, text bytes against stage 0; no `tween` symbol in any `hello`):

  | Build | Stage 0 | Now | Δ text |
  |-------|---------|-----|--------|
  | 3ds-release `hello.elf` | 216 084 | 216 260 | **+176** (budget 256) |
  | 3ds-dev `hello.elf` | 232 464 | 232 928 | +464 (assert strings of Development) |
  | pc-release `hello.exe` | 129 080 | 129 376 | +296 (x64 code; 40 B over the 256 target: **accepted by the maintainer** 2026-11-22, the 3DS is within budget) |
  | pc-debug `hello.exe` | 196 572 | 197 288 | +716 (unoptimized) |

  `hello.3dsx` (3ds-release) went from 261 460 to 261 640 B. `Scene` grew by one pointer (4 B on 3DS, 8 B on PC).
  `examples/scenes` screenshot checked (unchanged title screen); the platformer's `gate.nnhs` and `level.nnhs` pass
  with `nnh run platformer --pc --script`.
- **Hand mutation testing, stages 4–5** (same scratch runner): 40 mutants over sequence playing, layout, building,
  commands on sequences, the scene hooks, attach/detach/notify and binding. 26 were killed at once. 13 real
  survivors got new tests that kill them:
  - a call at the start of a restarted loop;
  - a looping yoyo step inside a sequence (forward and mirrored loops, ending on its start);
  - `on_loop` of a looping step;
  - a step back at its start is not written again;
  - a child callback that pauses its sequence;
  - a step joined to a later group is not written at append;
  - appending after the start;
  - a sequence longer than 1 hour;
  - a step looping forever (the old test hid it behind an earlier failure in the same set);
  - a node attached during a pass that the pass still reaches (two mutants);
  - the partial-sequence cleanup, visible only in Release, since Debug stops at the assert: checked with the pc-release
    tests.

  One was equivalent: marking a sequence's children Dead when it ends was dead code (nothing reads a child's state;
  freeing the sequence frees them), so `sequence_kill_children` was removed. Result: 39/39 non-equivalent mutants
  killed.

#### Stage 6 refinements and results (2026-11-22, implemented on PC)

- `examples/tweens` as planned, with one file fewer: the scene switcher is a plain `Demos` struct of scene pointers
  (`demos.hpp`), and each scene handles SELECT/START itself. The intro (`intro.{hpp,cpp}`) shows the sequence, a
  timer started from a call step ("READY") and an `nnh::Timer` blink; the overlay has its own bound set.
- The self-check (`self_check.{hpp,cpp}`, pure) plays a fixed script for 600 frames: tweens of the three kinds
  (elastic yoyo loops with `on_loop`, `by` with a delay, an infinite yoyo Color with `BackOut` overshoot, a looping
  setter), a yoyo sequence with a join, a wait, a call and `BackInOut`, `after`/`every`, `nnh::Timer` values, speed
  changes (0.5 at frame 150, 1.5 at 300, 1 at 450), a kill and a `complete()` of an unstarted sequence. FNV-1a over
  every value's bits, each callback's letter and `active()`, every frame. Golden checksum **`B44CA3F6`** (pc-debug
  and pc-release give the same). It also runs as `tests/tweens/test_self_check.cpp` (in `nnh_tests`) and as the
  ctest `tweens_self_check_pc` (the simulator, `TWEENS_DEMO=check`, Debug/Development).
- **`check.nnhs`**: the verdict line comes in the same frame as frame 600's checksum line, and `expect` matches only
  later frames, so the script waits for `tweens: PASS B44CA3F6` (which carries the same sum) instead of the 600
  line. `NNH_PRESET=pc-debug nnh run tweens --pc --no-build --script examples/tweens/tests/check.nnhs` passes (44
  lines, 1441 frames); its screenshots (showcase, overlay, self-check, bench) were read.
- PC bench (pc-debug): 256 tweens, average 0.06 ms, worst 0.23 ms per `update()`; the set's block is 57 440 B
  (224 B per slot on x64).
- Hand mutation testing of the self-check's logic (verdict, checkpoint cadence, end, rounding probe, mixing, the
  callback record, the FAIL text): 9/9 killed.

#### Stage 7 (prepared; the console run needs the maintainer)

- T7.1 done on Windows: `tweens.3dsx` builds in 3ds-release (291 612 B; `tweens.elf` text 245 700) and in 3ds-dev,
  the flavor `nnh run` uses (315 832 B; text 265 188), without warnings. The bench logs the set's block size on
  start (`tweens bench: started, 256 tweens, set block N bytes`): on the 3DS N is 256 × 184 + 68 = 47 172 B (plus
  alignment), which checks the 32-bit slot size on the console itself.
- T7.2, on the Old 3DS: open the Homebrew Launcher and press Y, then run
  `build/pc-debug/tools/nnh/nnh run tweens --script examples/tweens/tests/check.nnhs`. Expected in the saved log
  (`build/logs/tweens-*/`): `tweens: self-check started, rounding probe ok`, the nine checksum lines
  `tweens: 60 sum 7332AADE` … `tweens: 540 sum 65BFE3A6`, then `tweens: PASS B44CA3F6`, then
  `tweens bench: 256 tweens avg X.XX ms worst Y.YY ms (target < 1 ms) PASS` and `nnh: PASS`. Then a look at the
  showcase at 60 Hz with the debug overlay.

#### Final measurements (T8.3, 2026-11-22; Windows, 3 runs)

| Measure | Stage 0 | Now |
|---------|---------|-----|
| one-line TU: preprocessed lines · `-fsyntax-only` | `<nnh/events.hpp>` 7 633 · 0.38–0.45 s | `<nnh/tween.hpp>` 8 413 · 0.42–0.51 s |
| edit `examples/hello/main.cpp` → simulator (pc-debug) | 1.47 / 1.61 s | 1.45 / 1.35 / 1.37 s |
| edit `examples/hello/main.cpp` → `.3dsx` (3ds-release) | 2.59 / 2.92 s | 2.80 / 2.61 / 2.58 s |
| clean `nnh_core` (pc-debug, Ninja) | 3.2–3.4 s | 3.6–3.8 s (+14 tween/scene files) |
| `hello` text | see stage 0 | see the stage 5 table (+176 B on 3ds-release) |
| `tweens` | — | 3ds-release `.3dsx` 291 612 B, pc-release `tweens.exe` text 254 568 |

## Test strategy

All tests run on the fake platform (`ctest --preset pc-debug`). Stage ends are also run on VM 120 (GCC 13/14,
clang 18).

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/tween/test_ease.cpp`: 0 at 0, 1 at 1, 0 for NaN/negative/tiny t, within [0, 1] for non-overshooting curves (dense sweep), overshoot sign for back/elastic, against the `<cmath>` double reference (tests may use libm) within 1/4096; `test_ease_math.cpp`: approximation error bounds |
| FR-2 | `test_ease.cpp`: `ease()` without init; `Easing` from a custom function used by a tween (`test_tween_values.cpp`) |
| FR-3 | `test_ease_golden.cpp` (bit patterns on every host compiler, VM 120); self-check checksum on the 3DS (AC-5); T0.2 `objdump` check |
| FR-4 | `test_tween_set.cpp`: capacity, full set → empty handle + error log, or an assert caught by `platform_fake::run_expecting_crash` (as `tests/events/test_overflow.cpp`), destruction kills silently; `test_tween_alloc.cpp` (AC-4) |
| FR-5 | `test_tween_values.cpp`: float/Vec2/Color by pointer and by setter, Color rounding and clamping on back/elastic overshoot; `compile_fail_setter_capture.cpp` (capture > 16 B) |
| FR-6 | `test_tween_start.cpp`: to/by/from_to, capture at start and after a delay, capture once across loops, `from` written at creation, not with a delay |
| FR-7 | `test_tween_loops.cpp`: delay, n loops, infinite, restart, yoyo (mirrored values), loop counts |
| FR-8 | `test_tween_handles.cpp`: queries and commands, stale after finish/kill/clear/reuse, empty handle, handle of another set asserts |
| FR-9 | `test_tween_group.cpp`: pause/resume/kill all, `kill_target` (standalone and whole sequence), two tweens on one target (later wins) |
| FR-10 | `test_sequence.cpp`: append, join (group = longest), wait, call, sequence delay/loops/yoyo (reverse order), capture when reached, step limits, building allocates nothing |
| FR-11 | `test_tween_finish.cpp`: `on_finish` after the final write, `on_loop`, events delivered at the next `nnh::frame()`, nothing on kill/clear/auto-clear; `compile_fail_big_event.cpp` |
| FR-12 | `test_tween_reentrancy.cpp`: start/pause/complete/kill from callbacks (including self), fresh tweens count next update, killed not called, depth limit 8 with a logged error |
| FR-13 | `test_tween_timers.cpp`: `after`/`every` with `Duration` and `Frames`, n and forever, several fires per `advance`, event form |
| FR-14 | `test_timer.cpp`: once, repeating with several fires in one `advance`, pause/resume/stop/restart, remaining/elapsed/progress, no init needed |
| FR-15 | `test_scene_attachments.cpp` (D30 alone); `test_tween_scene.cpp`: updates before `update()`, only on top, cleared silently on pop/replace/end of `run()`/`shutdown()`, targets untouched afterwards, works after re-entering, hand update asserts (AC-3) |
| FR-16 | `test_frame_clock.cpp`: `Duration` → ticks table, 30/60 updates, three 0.5 s steps end on update 90, 10 loops of 1 s end on 600, `Frames{n}` ends on update n, same values/callback order across two identical runs (AC-2) |
| FR-17 | `test_frame_clock.cpp`: speed 0.5 doubles exactly, 0 freezes, 2 halves, `advance(Duration)` with remainder, set on a bound set asserts |
| FR-18 | `test_tween_misuse.cpp`: invalid times, absurd capacities, foreign handles, bound updates (assert caught by `platform_fake::run_expecting_crash`, or ignored + logged where asserts are off, as `test_overflow.cpp` checks both); profile scope name present (`profile::slowest` after an update, Development) |
| FR-19 | T5.4/T8.3: `hello` size and `nm hello.elf` / `hello.exe` show no `tween` symbols; `examples/scenes` and `examples/platformer` unchanged and passing their scripts (AC-8) |
| AC-5, AC-6, AC-7 | `examples/tweens`: `tests/tweens/test_self_check.cpp` (golden checksum on PC), screenshot read from PNG, `nnh run tweens --pc --script examples/tweens/tests/check.nnhs`, the same script on the Old 3DS (only HBL → Y) |

## Risks and alternatives

- **Determinism holes.** Fused multiply-add (closed by `-ffp-contract=off` + `objdump` check), libm (not used),
  subnormals (curves avoid them; documented limit for game values), a future change to libctru's FPSCR setup
  (caught by the self-check's rounding probe), and the game's own custom easings (outside the guarantee, documented).
  Alternative considered: all-integer fixed-point curves. Rejected because the tweened values are floats anyway, and
  with round-to-nearest on both sides IEEE float is already deterministic.
- **Header weight and code size in games.** Templates per value type and per start function. Mitigation: they only
  pack and forward. T0.1/T8.3 measure a one-file edit with `<nnh/tween.hpp>` and the example's size.
- **Dangling pointer targets.** This is a documented rule, not a check (spec answer). Mitigations: bound sets,
  `kill_target`, setters. The docs and the header say it in one sentence.
- **Sets alive after `shutdown()`.** They appear in the leak report. The docs and the example destroy them first.
- **Scene hook re-entrancy** (a set attached or destroyed during a pass). Covered by the epoch and cursor rules and by
  `test_scene_attachments.cpp`.
- **Sequence semantics** (callbacks in yoyo, `complete` on sequences, `kill_target` on a child). They are fixed above
  and match DOTween where it has an answer. If the maintainer prefers otherwise, they are small, local changes in
  `sequence_advance.cpp`.
- **The 1 ms budget on an Old 3DS.** The estimate is about half of it. The fallbacks are listed under Performance.
- **Tick resolution.** 1/4096 frame limits speeds to multiples of 1/4096 and times to 1 hour. That is ample for game
  tweens. Longer waits use the `Timer` value with `advance` or game counters.
- **Alternative for single tweens: a builder chain.** Rejected (see the API notes): a chain makes FR-6's
  creation-time write order-dependent. Builders are used only for sequences, where steps are appended in order
  anyway.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | No platform code. Bit-identical across PC compilers and the 3DS by own curves, `-ffp-contract=off` and libctru's round-to-nearest (verified); the subnormal limit is documented |
| II | ✅ | ≈ 184 B per slot and 8 B per step on 3DS, bounds `static_assert`ed for 32-bit; bench under 1 ms checked on the console |
| III | ✅ | Thin templates over `.cpp` functions, forward-declared `Color` and `Scene`, a detail header reusing headers that `events.hpp` already pulls; measured in T0.1/T8.3 |
| IV | ✅ | Game events on finish, queries for state, no engine event |
| V | ✅ | No allocation after creation (counted test), inline callbacks, no `virtual` per tween (one function-pointer call per bound set per frame) |
| VI | ✅ | Every FR has unit tests on the fake platform before code; the example adds the screenshot and the console run |
| VII | ✅ | 17 core files, each one concept, ≤ 200 lines planned |
| VIII | ✅ | `docs/api/tween.md`, scene/time links, architecture page for D30, example |
| IX | ✅ | Equations re-derived, own approximations, original example art (shapes) |
| X | ✅ | Misuse asserts/logs, stale handles harmless, `Scene` destructor catches a set that outlives its scene |
| XI | ✅ | D30 costs unbound scenes one pointer and a null check. The module is linked only when used (static library objects); easing and `Timer` work alone |

No exceptions to the constitution. D30 is the one new architecture decision.
