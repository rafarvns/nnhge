# Spec 015: Tweens and timers

Status: implemented (console T7.2 pending) · approved 2026-11-22 (maintainer pre-approved recommendations: every open question below is answered with
the recommended answer)
Author: NNHGE maintainers · Date: 2026-11-22
Depends on: [spec 001](../001-event-bus-and-scenes/spec.md) (event bus, scenes and the scene stack),
[spec 002](../002-core-foundation/spec.md) (`Duration`, memory tags, asserts, logs, profiling). Uses the value types
`Vec2` (`<nnh/math.hpp>`) and `Color` (`<nnh/gfx.hpp>`) without linking graphics code. Verified on the console with
[spec 013](../013-console-remote/spec.md)'s `nnh run --script`. Implements the ROADMAP item "Tweens and timers" of
"Features in the spirit of Godot and Unity". Optional for games (constitution XI, decision D25).

## Problem and goal

Every game animates values: a title that slides in, a coin that pops and fades, a menu cursor that eases between
rows, a screen flash, a "READY... GO!" that waits one second and then starts the level, an enemy that fires every
two seconds. Today each of those is a hand-written counter, a hand-written easing formula and a hand-written state
machine for "then do the next thing", repeated in every scene and easy to get subtly wrong (off by one frame, drift
between loops, a callback that runs after the scene is gone). Godot (Tween, Timer), Unity's DOTween and most 2D
frameworks solve this with tweens and timers. After this spec a game says "move this `Vec2` to there in 0.4 s with
`back_out`, then fade this colour, and 1 s later call me", inside its scene, with no allocation while playing; the
result is identical frame by frame on PC and 3DS (so replays and console tests compare exactly), and everything a
scene started stops by itself when the scene leaves the stack. A game that prefers its own counters ignores all of
it and pays nothing.

## User stories

- As a game developer, I want to animate a `float`, a `Vec2` or a `Color` of my scene from one value to another over
  a duration with a standard easing, so that UI and effects look polished without writing interpolation code.
- As a game developer, I want delays, loops, ping-pong (yoyo), sequences ("this, then that") and parallel groups
  ("these together"), so that a whole intro animation is a few lines instead of a state machine.
- As a game developer, I want "after 1 s, do this" and "every 2 s, do that" without counters, and a plain timer
  value I can poll when I prefer queries to callbacks.
- As a game developer, I want to be told when an animation finishes — by a callback in my scene or by one of my own
  events on the bus — so that the next step of my game can react.
- As a game developer, I want to pause, resume, finish early or kill an animation, one at a time, by target or all
  at once (a pause menu freezes the level; a dying enemy stops its tweens).
- As a game developer, I want the tweens and timers of a scene to stop and disappear when the scene leaves the stack,
  so that nothing writes into a scene that is gone or calls back into it later.
- As a game developer, I want tweens to advance by game frames, the same on every machine, so that my replays and
  console tests give the same numbers on PC and on the 3DS.
- As a game developer who prefers my own code, I want to use only the easing functions (or nothing at all) and lose
  nothing.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### Easing

- **FR-1 Standard easing set.** The engine provides these easings, each a function of progress `t` in [0, 1]:
  `linear`; and `in`, `out` and `in_out` variants of `quad`, `cubic`, `sine`, `back`, `elastic` and `bounce` (19
  in total), with the conventional shapes from Robert Penner's equations (back overshoot 1.70158; elastic with its
  usual period; bounce with four bounces). Every easing returns exactly 0 at `t = 0` and exactly 1 at `t = 1`; `back`
  and `elastic` may leave [0, 1] in between (overshoot), the others stay inside it.
- **FR-2 Easing is usable on its own.** A game can evaluate an easing directly (`ease(Ease::cubic_out, t)` or the
  equivalent the plan fixes), with no set, no tween and no engine state, so hand-written code can use the curves
  (constitution XI). A game can also pass its own easing as a plain function `float(float)` wherever an `Ease` is
  accepted.
- **FR-3 Deterministic everywhere.** For the same `t`, every easing returns the **same bits** on PC (every supported
  host compiler) and on the 3DS: the curves do not call the C math library (whose `sin`/`pow` differ between
  platforms); the engine computes them with its own documented, bounded approximations whose error against the exact
  curve is stated in the docs (target: below 1/4096 of the range, invisible on screen).

### Tweens

- **FR-4 Tween sets own tweens.** Tweens and timers live in a **tween set** that the game creates and owns (normally
  a member of the scene that owns the animated values; the plan fixes the name). A set has a **fixed capacity**
  given when it is created (tweens/timers and sequence steps); its storage is reserved once at creation (memory tag
  `tween`) and released when the set is destroyed; nothing allocates while tweens are started, played or finished.
  Starting a tween in a full set returns an empty handle, logs an error and asserts in Debug/Development (like a full
  event queue). Destroying a set kills everything in it silently.
- **FR-5 What a tween animates.** A tween animates one value of kind `float`, `Vec2` or `Color` from a start to an end
  value over a duration with an easing, through one of two **targets**:
  - **a pointer** to the game's value (`&title_pos_`): the tween writes the value after every update. Rule, stated
    in the docs and the header: the pointed-to value must outlive the tween — keep the set and its targets in the
    same object (FR-15 makes that automatic for scenes);
  - **a setter** callable `void(T)` stored inline without allocation (at most the event bus's handler size, today 16
    bytes: `this` plus one more pointer or id): the safe form for values that may disappear before the tween ends
    (a pooled enemy found by id each update; the setter does nothing if it is gone).
  `Vec2` interpolates each component; `Color` interpolates each channel and rounds to the nearest value, clamped to
  0–255 (so `back`/`elastic` overshoot never wraps a colour).
- **FR-6 Start and end values.** A tween goes **to** an end value or **by** a relative amount. Its start value is
  either given explicitly (`from`) or, by default, **captured from the target when the tween actually starts** (after
  its delay, or when its sequence reaches it), like Godot and DOTween. A tween with an explicit `from` and no delay
  writes `from` to its target as soon as it is created, so the first drawn frame is already right (no one-frame flash
  of the old value).
- **FR-7 Delay, loops, yoyo.** A tween may have a **delay** before it starts; a **loop count** (1 by default, n, or
  infinite); and a loop mode: **restart** (jump back to the start value) or **yoyo** (play forwards, then backwards
  with the mirrored easing, each direction counting as one loop). An infinite tween never finishes; it only stops when
  paused, killed or its set is cleared.
- **FR-8 Handles.** Starting a tween returns a small copyable **handle** (slot plus generation). Through a handle the
  game can query `running`, `paused`, `progress` (0–1 of the current loop) and `loops left`, and command `pause`,
  `resume`, `kill` (stop now, value stays where it is, no completion) and `complete` (jump to the end value — of the
  last loop for finite tweens — and run its completion, FR-11). A handle whose tween ended (finished, killed, set
  cleared, slot reused) is **stale**: queries answer "not running", commands do nothing — never a crash and never an
  effect on another tween. The default handle is empty and stale.
- **FR-9 Group commands.** A set can pause, resume and kill **all** its tweens, and kill every tween whose pointer
  target is a given address (`kill_target(&pos_)`, for a game object that is about to go away). Two tweens on the same
  target are allowed; both write each update in start order (the later one wins); the engine never kills one
  automatically.

### Sequences and parallel groups

- **FR-10 Sequences.** A game can build a **sequence**: an ordered list of steps that play one after another —
  a tween, a wait (`interval`), a call (`callback`, FR-11), or a tween **joined** to the previous step so that they
  play together (a parallel group; the group ends when its longest member ends). A sequence has its own handle (FR-8
  commands and queries apply to it as a whole), its own delay, loops and loop mode (yoyo plays the steps backwards
  in reverse order); its tweens capture their start values when the sequence reaches them (FR-6). The number of steps
  per sequence and in total per set is bounded by the set's capacities. Building a sequence allocates nothing.
- **FR-11 Completion: callbacks and events.** A tween or a sequence can have
  - an **on-finish callback** (inline, no allocation, same size rule as FR-5), called **during the set's update** in
    which it finishes, after its final value is written; and, for looping ones, an optional **on-loop** callback;
  - a **game event to publish on finish**: a copy of one of the game's own small trivially copyable events (size
    limit of an event, aim ≤ 32 bytes, compile-time check), published on the bus when it finishes, delivered like any
    event at the next dispatch (`IntroFinished{}` reaches other scenes and objects without them knowing the tween).
  `kill`, clearing a set and FR-15's automatic cancel run **no** callbacks and publish **no** events. The engine
  publishes no generic "tween finished" event of its own (one event per meaning, [events.md](../../docs/architecture/events.md)).
- **FR-12 Re-entrancy.** Callbacks may start, pause, complete or kill tweens of the same set, including the one
  calling. Rules (as for the event bus): tweens update in start order; a tween killed during an update is not updated
  or called afterwards; a tween started during an update begins counting at the **next** update (a `from` value is
  still written at once, FR-6); completing a tween from its own callback does nothing more. A callback chain that keeps
  completing tweens within one update is stopped by a documented depth limit with a logged error.

### Timers

- **FR-13 Timers in a set: "after" and "every".** A set can run a callback (or publish a game event) **after** a
  duration (`after(1s, fn)`), and **every** period for n repetitions or forever (`every(2s, fn)`). A timer is a
  tween with no value: it has a handle and the FR-8 commands and queries. Durations can be given as a `Duration` or
  as a whole number of **frames**.
- **FR-14 A plain timer value.** Separately from sets, the engine provides a small **timer value type** (Godot's
  Timer, but a plain struct): one-shot or repeating, started with a duration or a frame count, advanced by the game
  (`if (shot_timer.update()) fire();` — returns how many times it fired this update, so a repeating timer never loses
  a period), with `remaining`, `elapsed`, `progress`, `running`, `pause`, `resume`, `stop`, `restart`. It allocates
  nothing, needs no set, no scene and no `nnh::init()`, and follows the same clock rules (FR-16), so code that prefers
  "query for state" never needs callbacks.

### Scenes, ownership and the clock

- **FR-15 Scene-bound sets, automatic cancel.** A set can be **bound to a scene** when it is created (a member of
  that scene, given the scene). A bound set
  - is **updated automatically** once per frame, right before its scene's `update()`, and **only while its scene is
    the updating (top) scene** — a level under a pause menu freezes its tweens and timers; the pause menu's own set
    keeps playing;
  - is **cleared automatically** (FR-11: silently) when its scene leaves the stack (`pop`, `replace`, end of
    `run()`, `shutdown()`), so nothing it started writes into the scene or calls back into it after `on_exit`; tweens
    started in a later `on_enter` play normally;
  - must not also be updated by hand (asserts).
  An **unbound** set is updated by the game (`set.update()` once per frame, wherever it wants: a plain loop without
  scenes, a HUD object that lives across scenes, a game-wide transition layer). The engine keeps **no global set**
  and no `nnh::tween::to(...)` free functions with hidden state. A scene that doesn't bind a set pays nothing.
- **FR-16 Deterministic frame clock.** A set's clock advances only through its updates: each update advances it by
  exactly **one 60 Hz frame**, never by measured wall time, so a tween of 0.5 s takes exactly 30 updates on every
  machine, even when a frame is late. Rules the docs state and tests check: a duration of `n` frames ends on the
  `n`-th update after it starts; a `Duration` is converted to the frame clock with a documented rounding so that
  1 s = 60 updates exactly; leftover time carries into the next sequence step and the next loop, so a sequence or a
  looping tween never drifts (three 0.5 s steps end on update 90; 10 loops of 1 s end on update 600). Given the same
  calls in the same order and the same number of updates, a set produces the same values, the same callbacks in the
  same order and the same events, bit for bit, on PC and 3DS (FR-3) — replays (spec 012's) stay exact.
- **FR-17 Speed and explicit steps.** A set has a **speed** (1 by default; e.g. 0.5 for slow motion, 0 freezes it,
  2 fast) applied with exact, documented arithmetic (no accumulated float error), and the game may advance an
  unbound set by a given number of frames or by a given `Duration` (to drive it from `time::FixedStep`, to skip an
  intro, or — the game's choice, not deterministic — from `time::delta()`).

### Cost, safety, opt-in

- **FR-18 Bounded cost.** Updating a set costs time proportional to its active tweens, with no allocation, no
  virtual call per tween and no logging in the normal path; each set update is a profile scope (`NNH_PROFILE_SCOPE`).
  Capacities and limits are documented (FR-4, FR-12) and checked: misuse (full set, a handle from another set, a
  negative or absurd duration, updating a bound set by hand) asserts in Debug/Development and is ignored with an
  error log in Release, never undefined behaviour.
- **FR-19 Optional and independent.** Tweens and timers are their own optional module. A game that doesn't use them
  links no tween code (checked on executable size) and its scenes carry no extra cost beyond what the plan
  justifies for FR-15's binding. The module needs no other optional feature (tilemaps, data assets, sprite animation)
  and none needs it; easings (FR-2) and the timer value (FR-14) work without sets.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: every easing at 0, 1 and a table of points
  against golden values (exact bits) and against the exact curve within FR-3's error; float/`Vec2`/`Color` tweens by
  pointer and by setter (Color rounding and clamping on overshoot); `to`/`by`/`from`, start value captured at start
  and after a delay, `from` written at creation; delay, loops, infinite, restart, yoyo; handles (queries, pause,
  resume, kill, complete, stale handles after finish/kill/reuse, empty handle); `kill_target`, pause/resume/kill all;
  sequences with intervals, callbacks, joins (group length = longest), loops and yoyo of a whole sequence; on-finish
  and on-loop callbacks and events published on finish (delivered at the next `nnh::frame()`), none on kill/clear;
  re-entrancy (FR-12, including the depth limit); `after`/`every` with `Duration` and frames; the timer value type
  (one-shot, repeating with several fires in one update, pause, restart).
- **AC-2 Frame exactness.** Tests show: 30 updates for 0.5 s; 60 for 1 s; three 0.5 s steps end on update 90; 10 loops
  of 1 s end on update 600; speed 0.5 doubles them exactly; `n` frames end on update `n`.
- **AC-3 Scenes.** Tests with the scene stack on the fake platform: a bound set updates before its scene's `update()`,
  only while the scene is on top (not while a scene is pushed over it), is cleared silently on `pop`, `replace`,
  end of `run()` and `shutdown()` (no callbacks, no events, targets untouched afterwards), and works again after the
  scene re-enters; updating a bound set by hand asserts.
- **AC-4 No allocation while playing.** A test counts allocations: after a set is created, starting, playing,
  finishing, killing tweens and sequences and building sequences allocate nothing; the leak report is clean after
  sets are destroyed; a full set returns an empty handle and logs.
- **AC-5 Determinism on the console.** Example `examples/tweens` has a self-check mode that runs a fixed script of
  tweens, sequences, loops, yoyos and timers for a fixed number of frames, logs a checksum of all values and of the
  callback order every 60 frames, and ends with `tweens: PASS <checksum>` or `tweens: FAIL ...` against golden values
  recorded from the simulator. On an Old 3DS the maintainer only starts the game (Homebrew Launcher → Y) and
  `nnh run tweens --script <file>` (spec 013) waits for the PASS line and fails on a FAIL line; the same script passes
  with `--pc`. The verdict is in the saved log.
- **AC-6 Example `examples/tweens`** (original art or shapes only): top screen an easing gallery (one row per easing:
  its curve plotted and a marker moving along it in a loop); bottom screen a small intro built as a sequence (a panel
  slides in with `back_out` while a colour fades, a title pops, then "READY" after a timer), a repeating blink driven
  by the timer value type, and buttons to pause/resume, complete and restart, and to push a scene over it (its tweens
  freeze) and pop back. PC screenshot (`NNH_EXIT_AFTER_FRAMES` + `NNH_SCREENSHOT`, deterministic frame) checked by
  reading the PNG; a run on an Old 3DS at 60 Hz with the debug overlay.
- **AC-7 Cost on the console.** Measured on an Old 3DS with the profiler in the example's bench mode: updating 256
  active tweens (mixed kinds and easings, `elastic` included) stays under 1 ms per frame (target; the plan confirms
  it); the number goes in the docs.
- **AC-8 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS (no tween code
  linked, scenes unchanged in size or within the plan's justified cost); `examples/scenes` and `examples/platformer`
  keep working untouched.
- **AC-9 Docs.** `docs/api/tween.md` (easings with a picture of the curves, tweens, targets and the lifetime rule,
  sequences, callbacks vs events, timers, scene binding, the frame clock and determinism, capacities, costs), links
  from `docs/api/scene.md` and `docs/api/time.md`, a decision row in `docs/architecture/decisions.md`, CLAUDE.md
  layout and ROADMAP updated, `docs/REFERENCES.md` citing Godot Tween/Timer, DOTween and Penner's easing equations.

## Non-goals

- An editor, timeline UI or animation curves drawn by hand (constitution XI). Custom curves are plain functions (FR-2).
- Tweening other types (`int`, `Rect`, angles with wrap-around, strings), arbitrary user types through traits, paths
  and splines, Bézier curves, spring/physics-based motion — follow-ups if games ask (a setter on a `float` covers many).
- Nested sequences (a sequence as a step of another) and inserting steps at arbitrary times — joins cover parallel
  play; a follow-up if needed.
- Per-easing parameters (overshoot amount, elastic period), and the rarer Penner families (quart, quint, expo, circ) —
  reachable through a custom easing function.
- Sprite animation (frames, clips, state machines): its own ROADMAP item, which may use these timers.
- Scene transitions (fades between scenes) and camera shake as engine features: later ROADMAP items; the example
  shows how a game builds a fade with an unbound set.
- Saving and restoring tween state (save states, rewinding), automatic "safe mode" detection of dangling pointer
  targets, and an `on_kill` callback.
- Wall-clock timers that run while the game is suspended (HOME Menu): the clock is the game's frames.
- Threads: sets are used from the game's thread only.

## Constraints

- **I** — the same calls and the same values on PC and 3DS; the module is portable core code with no platform part.
- **II** — fixed capacities chosen by the game; storage reserved once per set (not on the console's small main-thread
  stack — libctru's default is 32 KB — when a scene with a large set is a local in `main`); bounded per-frame cost
  (AC-7).
- **III** — the public header stays light (no `<functional>`, no heavy templates in game headers); building
  `examples/hello` is unaffected.
- **IV** — completion is either a callback inside the owner's update or one of the **game's** events on the bus;
  state (progress, running) is queried, not published every frame.
- **V** — no exceptions, no RTTI, no `virtual` per tween, no allocation while playing (AC-4), callbacks stored
  inline.
- **VI** — the module is pure logic: everything is unit-tested first on the fake platform; the example is checked by
  screenshot and on the console.
- **VII** — easing, tween storage, sequences, timers and scene binding in small separate files; one public header.
- **VIII** — header comments, `docs/api/tween.md`, `examples/tweens`.
- **IX** — easing formulas are re-implemented from the published equations (Penner's are BSD-licensed); no code
  copied from GPL sources such as easings.net's.
- **X** — bugs are loud: misuse asserts in Debug/Development and logs in Release (FR-18); stale handles are harmless.
- **XI, D25** — entirely optional; easings and the timer value type work alone; plain counters stay fully supported.
- **Under pressure:** FR-15 needs a small, allocation-free hook in the scene module so a bound set is updated before
  and cleared on exit of its scene. It must stay generic (scenes must not depend on the tween module: XI, layering)
  and cost scenes that use no set at most a pointer; the plan proposes the mechanism and a decision row. FR-5's
  pointer targets rely on a documented lifetime rule the engine cannot check in general; FR-15 and setters are the
  mitigation.

## Prior art

- Godot `Tween` ([docs](https://docs.godotengine.org/en/stable/classes/class_tween.html)): tweens bound to a node
  (killed with it), `tween_property` capturing the start value when the tweener starts, `chain()`/`parallel()`,
  `tween_interval`, `tween_callback`, loops, pause modes; `Timer` node
  ([docs](https://docs.godotengine.org/en/stable/classes/class_timer.html)) and `SceneTree.create_timer`. FR-15 is the
  equivalent of a tween bound to a node and paused with the tree.
- DOTween ([documentation](https://dotween.demigiant.com/documentation.php)): getter/setter targets (FR-5's setter),
  `Sequence` with `Append`, `Join`, `AppendInterval`, `AppendCallback` (FR-10), `Complete`/`Kill`/`Pause`, loop types
  Restart and Yoyo, `SetLink` to tie a tween's life to an object (FR-15), "safe mode" for destroyed targets (our
  non-goal; setters instead).
- Robert Penner's easing equations ([robertpenner.com/easing](http://robertpenner.com/easing/), BSD licence): the
  standard curve set and its constants (FR-1).

## Open questions

All answered with the recommended answer, pre-approved by the maintainer (2026-11-22).

- [x] **Targets: pointer, handle, or value owned by the tween?** Options: (a) raw pointer only; (b) setter callable
      only; (c) both; (d) the tween owns the value and the game reads it through the handle every frame. — **answer:
      (c)** (FR-5). Pointers are the shortest code for the common case (a member of the scene that owns the set);
      setters handle objects that may vanish (look up by id, do nothing if gone) and values that aren't plain
      members. (d) forces a read per value per frame and still needs a pointer to write it somewhere; it can be done
      with a setter writing into a local.
- [x] **How are dangling targets prevented?** — **answer: by ownership, not by detection.** The set lives with its
      targets (same scene/object); a bound set is cleared when its scene exits (FR-15); `kill_target` for objects that
      die; setters for anything shorter-lived. A run-time check of every pointer is impossible in general and a
      partial one would give false confidence; the docs state the rule in one sentence.
- [x] **Global or scene-local sets?** — **answer: no engine-global set.** Sets are game-owned: bound to a scene
      (automatic update and cancel, FR-15) or unbound and updated by the game (FR-15), which covers "global" uses
      (HUD, transitions) explicitly. A hidden global set would outlive scenes and their targets (the bug this spec
      prevents) and is the singleton anti-pattern ([patterns](../../docs/architecture/patterns.md)).
- [x] **Who updates a set, and when?** — **answer:** bound sets: the engine, right before the scene's `update()`,
      only for the top scene (pause menus freeze the level for free); unbound sets: the game, once per frame. Updating
      before `update()` means values the scene reads in `update()` and draws in `draw()` are this frame's.
- [x] **Clock: frames or wall time?** — **answer: frames** (FR-16): each update is exactly one 60 Hz frame, durations
      in `Duration` are converted with documented rounding, leftovers carry over. Deterministic (replays, console
      checks), no drift, and consistent with game logic that also slows down when the console lags. `update(Duration)`
      (FR-17) covers FixedStep or deliberate wall-time use. Exact representation (e.g. integer microseconds or
      sub-frame fixed point) is the plan's.
- [x] **Bit-identical easing on PC and 3DS?** — **answer: yes, by not using libm** (FR-3): own approximations for
      `sin`/`exp2`-type terms (only `sine` and `elastic` need them; the rest are polynomials), golden-value tests on
      every target, and the plan checks compiler flags (no fused multiply-add contraction differences).
- [x] **Callbacks or events on finish?** — **answer: both, chosen per tween** (FR-11): an inline callback runs
      immediately inside the update (the owner reacting to its own animation), or the game's own event is published
      (others reacting, delivered next dispatch). No engine-generic `TweenFinished` event (one event per meaning).
      Kill/clear fire neither.
- [x] **Start value: captured when created or when started?** — **answer: when the tween starts** (after delay / when
      its sequence reaches it), as Godot and DOTween do, so chained moves start where the previous one ended; an
      explicit `from` with no delay is written immediately (FR-6).
- [x] **Storage: inline template capacity (`Tweens<32>`) or one block reserved at creation?** — **answer: one block
      reserved when the set is created** (tag `tween`, capacities from the game; FR-4). Scene objects stay small, so
      scenes kept as locals in `main` don't risk the 3DS main-thread stack, and the leak report and memory overlay see
      the cost. Creation happens in a scene's constructor or `on_enter`, never per frame. (The timer value type,
      FR-14, is a plain struct with no storage.)
- [x] **Default capacities and limits** — **answer:** a set's capacities are required at creation (no hidden
      default); documented ceilings: ≤ 1024 tweens/timers per set, ≤ 64 steps per sequence, ≤ 4096 steps per set;
      callbacks ≤ 16 bytes of captures (the bus's handler size); events published on finish ≤ 32 bytes; callback
      re-entrancy depth 8 (as `events::kMaxDepth`). The plan sizes one tween record and states the memory per entry.
- [x] **Easing set** — **answer:** linear + in/out/in_out of quad, cubic, sine, back, elastic, bounce (FR-1), plus
      custom functions (FR-2). Other Penner families are one custom function away (non-goals).
- [x] **Timers: only in sets, or also a value type?** — **answer: both** (FR-13, FR-14). `after`/`every` in a set for
      callback style; a plain timer struct for polling style (query for state, IV) and for code that uses no sets
      at all (XI).
- [x] **Sequence features** — **answer:** append, join (parallel with the previous step), interval, callback, loops
      and yoyo of the whole sequence (FR-10). Nested sequences and insert-at-time are non-goals for now.
- [x] **Module boundary and the `Color` dependency** — **answer: its own optional module `<nnh/tween.hpp>`**
      depending on the foundation, the event bus (publishing on finish) and the value types `Vec2` and `Color`.
      `Color` lives in `<nnh/gfx.hpp>`; including it links no graphics code, but the plan checks the header cost and
      may move the plain colour type to a small header (as data assets, spec 014, will need too) without changing
      game code.
- [x] **Example: new `examples/tweens` or extend an existing one?** — **answer: a new `examples/tweens`** (AC-5,
      AC-6): it shows every feature in one place and owns the console self-check; leaving `examples/scenes` and
      `examples/platformer` untouched shows XI. Using tweens in the platformer (coin pop, HUD) can follow as a small
      change.
- [x] **Scene hook mechanism** — **answer: the plan proposes it** (constitution pressure above), with these
      requirements: no allocation, no dependency from scenes on tweens, at most a pointer per scene, a decision row
      in `decisions.md`.
