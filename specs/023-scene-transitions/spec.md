# Spec 023: Scene transitions

Status: implemented 2026-11-24, Linux checks T9.1 passed 2026-11-24, console run 1 done 2026-11-24 (logic and doors PASS,
bench split and loading over target), optimisation round T10.3 done on the PC, console rerun pending (approved 2026-11-23 (overnight pre-approval by the maintainer: every "Decisions to take" item and open
question as recommended; to review in the morning). The decision row is D38 (D37 is spec 022's)
Author: NNHGE maintainers · Date: 2026-11-23
Depends on: [spec 001](../001-event-bus-and-scenes/spec.md) (the scene stack, deferred `push`/`pop`/`replace`,
`nnh::run()`, `scenes::update_and_draw()`; FR-10 and FR-11 add an exception to "the top scene updates every frame"),
[spec 004](../004-2d-graphics/spec.md) (shapes, layers, `ignore_camera`, per-screen recording and `max_draws`),
[spec 002](../002-core-foundation/spec.md) (asserts, logs, profiling) and [spec 003](../003-full-input/spec.md) (input
queries and events, which FR-12 can block). Uses the frame clock of `<nnh/time.hpp>` (`Frames`, `FrameTime`) and the
libm-free easings of [spec 015](../015-tweens-and-timers/spec.md) without needing a `TweenSet`. Works with
[spec 017](../017-input-actions/spec.md)'s scene-bound action contexts, D30's scene attachments (tween sets, worlds,
pools), [spec 018](../018-camera-helpers/spec.md)'s zoom and tall-world layout and [spec 008](../008-audio/spec.md)'s
music fades, without depending on any of them. Verified on the console with [spec 013](../013-console-remote/spec.md)'s
`nnh run --script`. Implements the ROADMAP item "Scene transitions" of M7. Optional for games (constitution XI,
decision D25).

## Problem and goal

Every game changes scenes — title to level, level to level through a door, game over back to the title — and today a
change is a cut: `scenes::replace(level)` swaps the picture between one frame and the next. Games that want a fade,
a wipe or the classic iris closing on the player must build it themselves, and it is easy to get subtly wrong: an
overlay object that has to live outside both scenes and draw over them, the swap done before the screen is fully
covered (one frame of the new level flashes), the old level still running and taking the player's input while it
fades, the A that confirmed "Start" making the player jump in the new level, a load in `on_enter()` that freezes the
fade halfway, timing measured in wall-clock seconds so a slow frame skips half of it and a replay never matches, and on
the 3DS doing all of it on two screens. Godot does it with a `CanvasLayer` fade plus `change_scene_to_file`, Unity with
`LoadSceneAsync` and a tweened canvas, HaxeFlixel with `FlxTransitionableState`. After this spec an NNHGE game asks for
a scene change **with a transition** in one call — a fade to a colour, a wipe, a split, blinds or an iris, on one or
both screens, or its own effect — and the engine covers the screen, changes the stack exactly when it is fully covered,
optionally runs the game's **loading work** over several frames (with progress for a loading indicator), reveals the
new scene, keeps input away from both scenes meanwhile, and does it all on the frame clock with the same frames and
pixels on PC and 3DS. A game that keeps calling `scenes::replace` pays nothing.

## User stories

- As a game developer, I want `replace(level)` with a fade to black (or white, or any colour) in one call, so that my
  title screen doesn't cut to the level.
- As a game developer, I want wipes, a split (doors closing), blinds and an iris closing on a point (the player), so
  that each game mode can have its own feel without writing the drawing code.
- As a game developer, I want the screen to be fully covered before the old scene leaves and the new one enters, so that
  no frame of the wrong scene ever flashes.
- As a game developer, I want neither scene to react to the player while the transition plays, and the button that
  started it not to leak into the new scene, so that a confirm press doesn't also jump.
- As a game developer, I want to load a level's assets while the screen is covered — across several frames, with a
  progress value I can show — so that a long load looks like a loading screen instead of a frozen picture.
- As a game developer, I want a dedicated loading scene (a tip, an animation, a progress bar) between two scenes, built
  from the same calls, so that big loads get a proper loading screen.
- As a game developer on the 3DS, I want the transition on both screens at once, on only one of them, or across both as
  one tall area (a wipe that runs down the top screen and continues on the bottom one), so that it matches my layout.
- As a game developer, I want to write my own transition (a sprite curtain, a logo that grows) that the engine times
  and orders for me, so that I'm not limited to the built-in effects.
- As a game developer, I want to know when the screen is covered and when the transition has finished — by a query,
  a callback or my own event — so that I can start the level timer, the music or a cut-in at the right moment, and fade
  the music out together with the picture.
- As a game developer, I want transitions to take exactly the same frames on PC and 3DS and in replays, so that console
  checks and replays compare exactly.
- As a game developer who prefers plain loops or my own overlay code, I want the effects usable as plain draw calls and
  `scenes::replace` to keep working as it does, so that I lose nothing.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### Starting a transition

- **FR-1 Transition requests.** The engine provides transition variants of the scene stack's changes: **replace** with
  a transition, **push** with a transition, **pop** with a transition, and a transition with **no stack change** (the
  screen is covered and revealed around a game callback — a room change inside one level, a "fade in" at game start
  with an empty cover phase). Each takes the target scene where the stack change needs one and a small **options**
  value (effects, durations, easings, screens, callbacks; FR-3 to FR-8, FR-13, FR-16) and returns whether the
  transition started. The names are fixed by the plan (working names: `transition::replace(scene, options)`,
  `transition::push`, `transition::pop`, `transition::cover`). The plain `scenes::push/pop/replace` keep their meaning
  and stay instant.
- **FR-2 Phases.** A transition runs four phases in order, each a whole number of frames:
  1. **Out** — the cover grows from nothing to full over the out duration;
  2. **Covered** — the stack change is applied (FR-11), then the optional loading work runs (FR-13) and the optional
     minimum hold elapses; the screen stays fully covered;
  3. **In** — the cover shrinks from full to nothing over the in duration;
  4. **Done** — the transition is over; the next frame is a normal frame.
  Any of out, hold and in may be zero (zero out: the screen is covered at once — a cut to the cover colour; zero in: a
  cut from the cover to the new scene). A transition with all three zero and no loading work behaves as the plain stack
  change plus one covered frame (documented).
- **FR-3 Timing on the frame clock.** Durations are `FrameTime` values (a `Duration` or `nnh::Frames{n}`), from 0 to
  10 seconds each. A transition advances by exactly one 60 Hz frame per frame, never by measured wall time: an out of
  0.5 s takes exactly 30 frames on every system, and a slow frame (a load in `on_enter()`, a debugger stop) delays it
  without skipping any of its steps. Each phase maps its time progress to coverage through an **easing** (FR-20; the
  default is linear), separately for out and in. The simulator's pause and frame step (spec 002) pause and step the
  transition with the game.

### Effects

- **FR-4 Built-in effects.** Every built-in effect is a function of **coverage** `c` in [0, 1] for a screen: at `c = 0`
  it draws nothing, at `c = 1` it covers the whole screen with its colour (opaque), and coverage only grows during out
  and only shrinks during in. Built with the engine's rectangles only (no shaders, no textures, no render targets):
  - **Fade** — the whole screen in the effect's colour with alpha `c` (rounded to 1/255; the plan fixes the rounding);
    a white fade is a flash;
  - **Wipe** — an opaque rectangle growing from one edge (left, right, top, bottom);
  - **Split** — two rectangles closing from opposite edges towards the middle (horizontal or vertical), "doors";
  - **Blinds** — N bars (2 to 32, default 8) each growing from its own edge, horizontal or vertical;
  - **Iris** — everything outside a **circle** or a **diamond** centred on a given screen point, whose radius shrinks
    to 0 at full cover (the closing circle on the player of countless platformers). It is drawn as horizontal bands of a
    fixed height (1 to 16 pixels, default 4), so its edge is a staircase of that step; the radius at `c = 0` is the
    distance from the centre to the farthest screen corner, so the iris starts fully open wherever the centre is.
  Every effect's geometry is computed in integers (screen pixels) from `c`, the screen size and the settings, so it is
  the same on every system (FR-20).
- **FR-5 Reveal.** By default the in phase plays the out effect backwards (a wipe from the left recedes to the left, an
  iris reopens on the same point). The options may give the in phase its own effect (fade out, wipe in), its own colour
  and, for an iris, its own centre (close on the door, open on the player's new position); directional effects have a
  **continue** option for the reveal, where the cover keeps travelling in the same direction and leaves by the opposite
  edge.
- **FR-6 Screens.** One transition has **one timeline for both screens**. For each screen the options say what it shows:
  the same effect as the other screen (the default: both screens), its own effect and colour, or **none** (that screen
  is never covered — a transition on the top screen only while the bottom keeps its map). An option draws directional
  effects and the iris **across both screens as one tall area**, laid out as spec 018's tall world (the bottom screen
  under the top one, centred, 40-pixel inset each side, a gap setting in pixels, default 0): a wipe from the top runs
  down the top screen and continues on the bottom one, an iris on a point of the bottom screen closes over both.
- **FR-7 Where the overlay draws.** The transition's overlay is drawn after every scene's `draw()` of the frame, in
  screen pixels: it ignores `Camera2D` and its zoom, it covers every game layer the scenes drew (including layer 15),
  and the debug text and overlay stay above it. Game code that draws after `scenes::update_and_draw()` in a hand-written
  loop is drawn after the overlay (documented, as a way to keep a HUD over a transition).
- **FR-8 Custom effects.** A game may give, for out, in or both, its **own effect**: a plain function (or a lambda with at
  most 16 bytes of captures, like spec 015's callbacks) called once per frame per covered screen with the screen, the
  coverage, the phase, the frames elapsed in the phase and, while covered, the loading progress (FR-13). It draws with
  the public `gfx` calls (textures, sprites, text — a curtain made of the game's art, a logo that grows, a progress bar)
  and is drawn where FR-7 says. The engine times it, orders it, blocks input (FR-12) and changes the stack exactly as
  for built-in effects; whether the custom effect really covers the screen at `c = 1` is the game's responsibility
  (documented: a custom effect that doesn't cover will show the stack change).
- **FR-9 Effect geometry as a pure query.** For each built-in effect the engine offers a pure function that writes the
  rectangles (and colour) the effect draws for a screen at a given coverage into a caller-provided buffer and returns
  how many, with a documented maximum per effect, and a plain draw call that draws an effect at a coverage now. Both
  work with no transition running and no scene stack (a game with its own overlay code, a menu's background wipe, a
  pause fade) and are what the transition itself draws, so the self-check (AC-6) can checksum exactly what is drawn.

### Scenes, the stack and input during a transition

- **FR-10 What the scenes do.** While a transition runs:
  - **out**: the stack is unchanged and every scene draws as usual; by default **no scene updates** (the outgoing scene
    freezes under the growing cover);
  - **covered**: the stack has changed; scenes draw as usual (under the cover); no scene updates;
  - **in**: every scene draws as usual; by default **no scene updates** — the incoming scene's first `update()` comes in
    the frame after the transition is done;
  - options let the top scene keep **updating during out** (gameplay continuing while a slow fade starts) and the new
    top scene **update during in** (an intro animation playing as the iris opens); scene-bound attachments (D30: tween
    sets, worlds, pools) are notified exactly when their scene's `update()` runs, so a frozen scene's bound tweens freeze
    with it, as under a pause menu.
- **FR-11 The stack change.** The transition's stack change is requested on the frame the cover becomes full and is
  applied by the scene stack's normal deferred path (spec 001): the same `on_exit()`, `AfterExit` attachments,
  `on_enter()` order, the same limits and ownership checks (a scene waiting in a transition counts as waiting to be
  pushed: destroying it, or pushing it elsewhere meanwhile, is caught like a pending push). Guarantees, checked frame by
  frame: no presented frame shows a covered screen's incoming scene before the cover is full, and none shows the
  outgoing scene after it was full. Work in the incoming scene's `on_enter()` may take as long as it needs; the screen
  stays covered and no transition frame is skipped (FR-3).
- **FR-12 Input is blocked.** By default, from the frame a transition starts until the frame it is done, game input is
  **blocked**: input queries read idle (no button held, sticks centred, no touch, no edges), no `ButtonPressed`,
  `ButtonReleased`, touch or action events are queued, and spec 017's actions read idle. When the block ends, every
  control held at that moment stays blocked until it is released (as spec 017's FR-13 does for contexts), so the press
  that started a transition — or one held through it — never reaches the new scene. Remote input from `nnh run
  --script` is game input and is blocked the same way; engine controls (the simulator's pause, step and window keys,
  the debug overlay toggle) are not. An option turns the block off for a transition (a game whose scenes update during
  it and want input). The block is part of the input module and available to games on its own (FR-23, decisions).
- **FR-13 Loading while covered.** The options may give a **loading step**: a function (or a 16-byte-capture lambda)
  called once per frame during the covered phase, after the stack change, that does a slice of the game's loading work
  and returns its progress in [0, 1] and whether it is done. The covered phase lasts until the step reports done
  **and** the minimum hold has elapsed (a hold avoids a flicker when the load is instant; default 0). The progress is
  passed to the custom effect (FR-8) and returned by the queries (FR-15), so a loading indicator can be drawn on the
  cover. The engine never loads on a thread (non-goals): the work runs inside the frame, the game decides its slice
  size, and a slice that takes longer than a frame only makes that frame long. A step still not done after 10 seconds
  logs one warning in Debug/Development (a loader that never finishes is a bug), and keeps being called.
- **FR-14 Loading scenes.** A dedicated **loading scene** works with no extra API: a transition replaces the outgoing
  scene with the loading scene, the loading scene's `update()` does the work in slices and shows its progress, then
  starts a second transition to the real scene. The docs and the example show this pattern, and the guarantees of
  FR-10 to FR-12 hold for both transitions.

### Queries, notifications, music

- **FR-15 Queries.** The engine answers: whether a transition is running; its phase; the coverage per screen this
  frame; the time progress of the current phase; the loading progress; the frames elapsed in the transition; and the
  out, hold and in durations of the running transition (so a game can fade its music over the same time, FR-17). State
  is queried, never published every frame (constitution IV).
- **FR-16 Notifications.** The options may give an **on-covered** callback (run after the stack change, before the
  loading step's first call — for a no-change transition this is where the game moves to the new room) and an
  **on-finished** callback (run in the frame the transition is done, before the next scene update), each a plain
  function or a 16-byte-capture lambda, and a **finish event** of the game's own type (trivially copyable, at most 32
  bytes, like spec 015's `.publish`) queued when the transition is done. The engine publishes no event of its own.
  Callbacks may query the transition; starting another transition from on-finished is allowed and starts it in the next
  frame (FR-19).
- **FR-17 Music.** Transitions don't depend on the audio module (XI). Fading or crossfading the music with the picture
  is the game's call at the moment it chooses — when it starts the transition (`music::stop(out seconds)`), from
  on-covered (`music::crossfade(...)` while covered, or `music::play(..., {.fade_in = in seconds})`) — with the
  durations from FR-15; the docs show the patterns and the example uses one (AC-5).

### Without scenes, interruptions

- **FR-18 Plain loops.** A game that drives its own loop with `scenes::update_and_draw()` gets transitions exactly as
  under `nnh::run()`. A game without scenes may still use FR-9's effect drawing and its own timing; a no-change
  transition (FR-1) also works for it if it calls the engine's per-frame transition step (the plan names it) — scenes are
  not required (XI).
- **FR-19 One at a time and interruptions.**
  - A transition request while another runs is **refused**: it returns false, changes nothing and logs once at debug
    level (a door entered twice in one frame is normal, not a bug). A request from on-finished starts in the next frame.
  - Plain `scenes::push/pop/replace` calls during a transition are applied as usual (documented); the transition's own
    change is applied to whatever the stack is when the cover is full (a pop with a transition pops the top scene of
    that moment).
  - A transition that pops the last scene ends the game loop after its covered phase (`run()` returns; the in phase is
    skipped).
  - `nnh::shutdown()` (and the end of `run()`) during a transition cancels it silently: no callbacks, no event, a
    pending scene is not pushed, the input block is lifted.

### Determinism, cost, safety, opt-in

- **FR-20 Same frames, same pixels.** The same calls give, frame by frame, the same phases, coverage values (bit for
  bit), effect rectangles and stack-change frames on PC (every supported host compiler) and on the 3DS. Easings are spec
  015's libm-free curves (same bits everywhere); coverage and geometry use documented integer and float operations
  without the C math library and without fused multiply-add differences (as specs 015 and 018). The fade's blend is done
  by each GPU; its colour may differ by at most one step per channel between systems (documented; geometry never
  differs).
- **FR-21 Bounded cost, no allocation.** The transition state is a fixed-size value in the module (target: at most 256
  bytes, the plan states it); nothing allocates, ever — not when a transition starts, not per frame. A frame of a
  transition costs a fixed amount of work plus its overlay draws, no `virtual` call, no logging in the normal path, and
  is a profile scope. Every built-in effect at its default settings draws **at most 128 rectangles per screen** (an
  iris with 1-pixel bands draws more; each effect's maximum is documented); overlay draws count against
  `AppConfig::gfx.max_draws` like any draw (documented; the default leaves ample room).
- **FR-22 Misuse is loud in Debug, harmless in Release.** Durations out of range or NaN, a negative hold, an iris centre
  that is NaN, a band height or bar count out of range, a pushed scene already on the stack, pop with a transition on an
  empty stack — assert in Debug/Development and are clamped or refused (with an error log) in Release; never undefined
  behaviour.
- **FR-23 Optional and light.** Transitions are an optional module with one public header that needs neither the
  tween module's sets, the audio module, tilemaps nor actions. A game that doesn't use transitions links no transition
  code and its frames cost what they cost today (the scene stack pays at most one null pointer check per frame for the
  hook it calls, as decisions D30 and D32 did); `examples/hello`'s executable size is unchanged. The input block (FR-12)
  is a small addition to the input module that costs games which never block one branch per frame.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: phases and their exact frame counts (out, hold, in,
  zero durations, all zero), easing per phase, coverage monotonic; every built-in effect's rectangles at coverage 0, 1
  and fixed points between (wipe four ways, split both ways, blinds 2/8/32 bars both ways, iris circle and diamond at a
  centre inside, on an edge and outside the screen, band heights 1, 4, 16), the reveal reversed, continued and with its
  own effect, colour and iris centre; per-screen effects, a screen with none, the tall area across both screens with
  and without a gap; overlay ordering (after every scene's draws, above layer 15, below debug text, unzoomed and
  ignoring the camera); custom effects (called once per covered screen per frame with the documented arguments);
  replace, push, pop and no-change transitions, under `nnh::run()` and in a hand-written loop, and FR-9's effect
  drawing with no scene stack at all (FR-18); which scenes update in each phase with and without the update options,
  and D30 `BeforeUpdate`/`AfterExit` notifications (a bound tween set freezes while its scene doesn't update);
  ownership checks for a scene waiting in a transition; the loading step (called after on-covered, progress passed to
  the custom effect and queries, minimum hold, the 10-second warning); queries; callbacks and the finish event
  (exactly once, at the documented frames; none on cancel); refused second requests, plain stack changes during a
  transition, pop of the last scene, shutdown during each phase; input blocking (queries idle, no button, touch or
  action events, held controls blocked until released at the end, remote input blocked, the opt-out); misuse cases
  (FR-22). The Linux build VM runs them with GCC 13/14 and clang 18 and with ASan/UBSan, as previous specs did.
- **AC-2 No wrong frame.** A fake-platform test runs replace, push, pop and no-change transitions with every built-in
  effect (and a custom one) and, for every frame, checks the recorded and submitted draw data: while the outgoing scene
  is drawn, the overlay's coverage is below full only before the stack change; from the first frame the incoming scene
  is drawn until the cover starts shrinking, the covered screens are entirely covered by the overlay; and no frame
  submits the incoming scene's draws on a covered screen without a full cover above them.
- **AC-3 Golden values.** A test plays a fixed script of transitions over 1200 frames — every built-in effect, both
  reveal styles, per-screen and tall-area variants, easings, a loading step of a fixed number of slices, zero-length
  phases, a refused request and a plain stack change mid-way — and checksums, per frame, the phase, each screen's
  coverage bits, the effect rectangles from FR-9 and the frames on which `on_exit`, `on_enter`, the callbacks and each
  scene's first `update()` happen. The checksum equals a golden value on every host compiler and in the console
  self-check (AC-6).
- **AC-4 Input.** Besides AC-1's unit tests, a fake-platform test with an `ActionSet` whose contexts are bound to the
  outgoing and incoming scenes: the confirm press that starts the transition, a button held through it and a tap during
  it produce no action, button or touch event and no `pressed()` in either scene, and the held button reaches the new
  scene only after it is released and pressed again.
- **AC-5 Example `examples/transitions`** (original art or shapes only), modes chosen by `TRANSITIONS_DEMO`
  (`showcase` (default) | `check` | `bench`): a title scene and two "rooms" drawn with shapes, tile layers or sprites on
  both screens and a moving square the player steers. Buttons cycle the effect (fade, wipe, split, blinds, iris circle
  and diamond, a custom curtain drawn with the example's own sprites), the reveal style, the screens (both, top only,
  tall area) and start a room change; a touch on the bottom screen sets the iris centre; one room change goes through a
  **loading step** (a fixed amount of work in slices, a progress bar drawn by the custom effect, a minimum hold) and one
  through a **loading scene** (FR-14); the example plays a short original music loop and fades it with the picture
  (FR-17); the HUD shows the phase, coverage and loading progress from the queries. PC screenshots
  (`NNH_EXIT_AFTER_FRAMES` + `NNH_SCREENSHOT`, deterministic frames) at mid-coverage of each built-in effect, of the tall
  area wipe and of the loading bar, checked by reading the PNGs (each effect's shape, both screens, no gaps in the
  cover at full coverage).
- **AC-6 Console script.** The `check` mode runs AC-3's script on the engine's real transitions and logs
  `transitions: <frame> sum <checksum>` every 120 frames and `transitions: PASS <checksum>` or `transitions: FAIL ...` at
  the end, against golden values recorded from the simulator. `examples/transitions/tests/check.nnhs` starts it,
  expects every line, then switches to the showcase, presses the buttons that start a few transitions (one with a
  loading step), checks that a button pressed during each one changes nothing (the HUD logs every input the scenes see:
  none until the transition is done), **touches** a point and checks the logged iris centre, and takes screenshots at
  fixed frames of mid-coverage, full cover and the loading bar. On an Old 3DS the maintainer only opens the Homebrew
  Launcher and presses Y, and `nnh run transitions --script examples/transitions/tests/check.nnhs` passes; the same
  script passes with `--pc`. The verdict, log and screenshots are in the saved log folder, and the console's screenshots
  show the same geometry as the PC's at the same frames.
- **AC-7 The platformer adopts transitions.** `examples/platformer`'s **doors** change level with an iris closing on the
  player and reopening on the new level's spawn point (both screens), with input blocked meanwhile and the music, if any,
  faded. The **replay's** level switch and the SELECT/START demo cycling stay instant (`scenes::replace`), so the
  replay's checksum lines in `examples/platformer/tests/level.nnhs` and the ctest `platformer_replay_matches_script` pass
  **unchanged** — no golden line is regenerated; the benchmark, gate, zoom and flip demos are untouched. A PC screenshot
  taken during a door's iris (the plan picks a start position or a scripted run that reaches a door) is checked by
  reading the PNG.
- **AC-8 Cost on the console.** The `bench` mode, measured on an Old 3DS with the profiler and logged as
  `transitions bench: PASS <numbers>` (or FAIL). **Targets** (the plan confirms them before code; they predate any
  console measurement), CPU per frame for the transition step plus recording and submitting its overlay, both screens:
  - fade, wipe or split: **under 0.1 ms**;
  - blinds with 8 bars: **under 0.3 ms**;
  - iris (circle, 4-pixel bands, centre in the middle — the heaviest default): **under 2 ms**;
  - the frame with the loading step: the engine's own share (excluding the game's slice) **under 0.05 ms**;
  - `examples/platformer`'s level 1 at 60 Hz through ten door irises (no frame over 25 ms, as the replay judges).
  If a target is missed, the implementation is optimised first; if it is still missed, the budget is set from the first
  console measurement with margin and written in the spec and the docs with the measured numbers (the maintainer's policy
  for benches, as in specs 019, 020 and 021). Spec 012's tile gate and the platformer's 60 Hz replay verdict still pass.
- **AC-9 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS (no transition code
  linked; any growth from the scene stack's hook or the input block is stated by the plan and measured);
  `examples/scenes`, `examples/sprites`, `examples/tweens`, `examples/actions` and `examples/camera` keep their
  screenshots pixel-identical and their scripts passing; every earlier spec's tests pass unchanged. Building a game
  `.cpp` that includes the transition header stays within constitution III's budgets (measured before and after on
  `examples/transitions`).
- **AC-10 Docs.** A new `docs/api/` page for transitions (requests and phases, the frame clock, every built-in effect
  with a picture, reveal styles, screens and the tall area, custom effects, the pure geometry query, what scenes do in
  each phase, input blocking, loading steps and loading scenes, queries and notifications, music patterns,
  interruptions, determinism, costs); `docs/api/scene.md` and `docs/architecture/scenes.md` (the frame flow with a
  transition, the hook), `docs/api/input.md` (the block), `docs/api/actions.md` (actions under a block),
  `docs/api/audio.md` (link to the music patterns) updated; a decision row in `docs/architecture/decisions.md` for the
  scene stack's transition hook and the input block if the plan takes them; CLAUDE.md layout and ROADMAP updated;
  `docs/REFERENCES.md` citing the prior art below.

## Non-goals

- **Effects that need the previous picture or render targets**: a crossfade or dissolve between two live scenes, a
  zoom, pixelate, blur or ripple of the old scene, a "screenshot slides away" — they need render-to-texture (a second
  colour buffer per screen on the 3DS, ~380 KB of VRAM for the top screen alone) and a submit path the renderer doesn't
  have. A follow-up spec if games ask; the custom effect (FR-8) covers anything drawable over the scenes.
- **Shader and mask transitions** (Unity UI masks, Godot shader transitions with a gradient texture and a cutoff):
  citro2d has no fragment shaders; a texture-based mask could come with render targets.
- **Threaded or asynchronous loading**: the engine has no loader thread and the 3DS's second core is not used; loading
  is spread over frames by the game's own slices (FR-13). Streaming loaders belong to a later spec (ROADMAP M4's
  "streaming on a thread for long loads").
- **Per-scene default transitions** set on the scene (HaxeFlixel's `defaultTransIn`, Monarch's per-screen transition
  handlers): a game keeps its options as a constant and passes them (decisions).
- **Several transitions at once, queued transitions, different timelines per screen**: one transition, one timeline.
- **Animating objects in and out** (a menu sliding in, a title dropping): spec 015's tweens do it inside the scene.
- **Stereoscopic depth of the overlay** (M8): the overlay will sit at screen depth when stereo arrives.
- **Built-in loading UI** (a default progress bar or spinner): the engine shows nothing of its own (XI); the example
  shows one.
- **Engine events** for transition phases (`TransitionStarted`, `TransitionFinished`): state is queried and the game
  may ask for its own event (FR-16).

## Constraints

- **I** — the same calls give the same frames and geometry on PC and 3DS (FR-20); the module is portable core code
  with no platform part (rectangles only); the example and scripts run in the simulator and on the console.
- **II** — no memory beyond a fixed state of at most 256 bytes (FR-21); overlay draws bounded per effect and counted in
  `max_draws`; costs measured on an Old 3DS (AC-8).
- **III** — one light public header (the scene and graphics headers, the frame clock, the easing curves; no tween set,
  audio or standard containers); `hello` unaffected; build times measured (AC-9).
- **IV** — transitions are queried; the engine publishes no transition events; a game's own finish event goes through
  the bus like any event (FR-16); scenes still talk only through events and stack requests.
- **V** — no exceptions, no RTTI, no `virtual` (effects and callbacks are plain function pointers with inline
  captures), no allocation at all.
- **VI** — phases, geometry, ordering, scene rules and input blocking are pure logic, unit-tested first on the fake
  platform (AC-1 to AC-4); the visible result is checked by screenshots and on the console (AC-5, AC-6).
- **VII** — requests and phases, each effect family, the overlay, the loading step and the input block in small
  separate files; one public header for the module.
- **VIII** — header comments, a `docs/api/` page, `examples/transitions` and the platformer updated (AC-10).
- **IX** — written from the published designs (Godot, Unity, HaxeFlixel, Defold Monarch, citro2d's fade); no code
  copied; original art only; credited in `docs/REFERENCES.md`.
- **X** — no new parser or file access; misuse is loud in Debug/Development and harmless in Release (FR-22); a stuck
  loader is reported (FR-13).
- **XI, D25** — optional: `scenes::replace` stays instant and fully supported, the effects work without transitions
  (FR-9) and transitions without tweens, audio, tilemaps or actions (FR-23).
- **Under pressure:**
  - **Spec 001's contract** "the top scene updates once per frame" gains an exception while a transition runs (FR-10),
    and the scene stack gains a second, global hook beside D30's per-scene attachments: the plan must keep its cost for
    games without transitions at one null check (as D32) and record a decision.
  - **The input module** gains a blocking state (FR-12) that also silences spec 017's actions and D28's remote input
    during a transition: the plan must show that action evaluation, the "blocked until released" rule and remote
    scripts compose (AC-4, AC-6) and that the input path costs games that never block one branch.
  - **XI and spec 015's header**: transitions use spec 015's easings but must not need its tween sets; if including
    `<nnh/tween.hpp>` would pull the set API into the header, the easing curves move to a light header of their own (as
    D31 moved the frame clock to `<nnh/time.hpp>`), with `<nnh/tween.hpp>` still providing them.
  - **Hidden state**: the running transition is module state, like the scene stack itself; it is not a game-level
    global, but the plan must justify it against the "no hidden globals" rule (decisions).
  - **`max_draws` is shared**: a game close to its draw capacity can overflow during an iris; FR-21 bounds and documents
    each effect's count rather than reserving capacity.
  - **AC-8's** targets predate any console measurement and fall back to the measurement-with-margin policy.

## Prior art

- **Godot**: `SceneTree.change_scene_to_file()` / `change_scene_to_packed()` swap the scene (deferred); the common fade is
  an autoload `CanvasLayer` on a high layer with a full-screen `ColorRect` animated by an `AnimationPlayer` or a `Tween`,
  changing the scene once the screen is black and playing the animation backwards; background loading with
  `ResourceLoader.load_threaded_request()` / `load_threaded_get()` —
  [`SceneTree`](https://docs.godotengine.org/en/stable/classes/class_scenetree.html),
  [Shaggy Dev: scene transition effects](https://shaggydev.com/2022/06/13/godot-scene-transitions/),
  [Catlike Coding: map transitions](https://catlikecoding.com/godot/true-top-down-2d/7-map-transitions/),
  [EasyTransition addon](https://github.com/IUXGames/EasyTransition). FR-7 (overlay above every scene), FR-11 (swap at
  full cover) and the known pitfall of the overlay living inside the scene being freed (the engine owns it here).
- **Unity**: `SceneManager.LoadSceneAsync()` with `AsyncOperation.allowSceneActivation = false` holds activation at
  progress 0.9 while a loading screen finishes, a common pattern with a minimum loading time; fades are usually a
  `CanvasGroup` alpha tweened with DOTween (`DOFade`) —
  [`allowSceneActivation`](https://docs.unity3d.com/ScriptReference/AsyncOperation-allowSceneActivation.html),
  [`LoadSceneAsync`](https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.LoadSceneAsync.html),
  [DOTween](https://dotween.demigiant.com/documentation.php). FR-13's progress, "done" and minimum hold.
- **HaxeFlixel**: `FlxTransitionableState` with `transIn`/`transOut` `TransitionData` (type fade or tiles, colour,
  duration, direction for a wipe, tile graphics circle/diamond/square, region), and global defaults —
  [`TransitionData`](https://api.haxeflixel.com/flixel/addons/transition/TransitionData.html),
  [`TransitionType`](https://api.haxeflixel.com/flixel/addons/transition/TransitionType.html),
  [transitions demo](https://github.com/HaxeFlixel/flixel-demos/blob/dev/Effects/Transitions/source/MenuState.hx).
  FR-4/FR-5's out and in effects with a direction; per-state defaults are a non-goal.
- **Defold Monarch**: a screen stack whose show/hide waits for per-screen transitions (`show_in`, `show_out`,
  `back_in`, `back_out`; fades and slides with easings), with custom transitions that report done —
  [Monarch](https://github.com/britzl/monarch),
  [forum: using Monarch transitions](https://forum.defold.com/t/solved-clear-steps-to-use-the-monarch-transition-feature/81109).
  FR-1's transitions on push/pop/replace of a stack; FR-8's custom effects timed by the engine.
- **citro2d** (the 3DS library NNHGE draws with): `C2D_Fade(color)` blends every following draw with a colour, its
  alpha the strength — a whole-screen fade in hardware —
  [base.h](https://citro2d.devkitpro.org/base_8h.html). FR-4's fade uses a rectangle instead so the PC and the fake
  platform draw the same thing; the plan may use `C2D_Fade` on the 3DS if it measures cheaper and looks the same.
- **Butano** (GBA): fades as actions over frames (`bg_palettes_fade_to_action`, a fade colour and an intensity), the
  frame-counted style of FR-3 —
  [colour palettes module](https://gvaliente.github.io/butano/group__palette.html).
- Classic platformers' iris-out on the player and DS games' wipes across both screens: FR-4's iris and FR-6's tall area.

## Decisions to take

Every question has a recommended answer; none is decided until the maintainer approves the spec.

- [x] **Where does the running transition live?** Options: (a) **module state in the engine, one transition at a
      time, like the scene stack**; (b) a `Transitions` object the game creates and passes around; (c) a D30 attachment
      on the outgoing scene. — **recommended: (a)** (FR-1, FR-19). A transition spans two scenes and the stack itself is
      engine state; (b) needs the object in both scenes and in the stack's path anyway, (c) dies with the outgoing
      scene's `on_exit()`, exactly the Godot pitfall. Module state is only linked when used (FR-23).
- [x] **API shape** — **recommended: transition variants of `push`/`pop`/`replace` plus a no-change `cover`, each with
      an options value (designated initializers), returning `bool`** (FR-1). It mirrors the stack calls games already
      know; a no-change variant covers room changes and the start-of-game fade in. Names fixed by the plan.
- [x] **How does the scene stack run the transition?** Options: (a) **a hook in `scenes::update_and_draw()`, a function
      pointer set while the module is in use (D32's style), that decides which scene updates and draws the overlay after
      every scene**; (b) the game calls `transition::update()`/`draw()` in its loop; (c) D30 attachments. —
      **recommended: (a)** (FR-7, FR-10, FR-18). (b) is the forgettable call D30 and D32 removed and can't stop the top
      scene's update; (c) is per scene and runs before `update()`, not after the draws. A new decision row (D38) records
      it; the scene module never names the transition module.
- [x] **Which effects are built in?** — **recommended: fade, wipe, split, blinds and iris (circle and diamond), plus
      custom effects** (FR-4, FR-8). They are the classic set (HaxeFlixel's fade and tiles, Monarch's fades and slides,
      the platformer iris) and all are rectangles. A tiled/checker dissolve is a follow-up (many cells, a few hundred
      draws at full cover).
- [x] **How is the iris drawn without shaders?** Options: (a) **horizontal bands of a fixed height computed in
      integers (default 4 pixels, settable 1–16)**; (b) a circle mask texture made at first use and scaled, plus four
      rectangles; (c) no iris. — **recommended: (a)** (FR-4, FR-21). Exact and identical everywhere, no texture memory,
      bounded draws (≤ 122 per screen at 4 pixels); the staircase suits pixel art. (b) is smoother but blocky at large
      scales with nearest filtering, costs texture memory and depends on each GPU's sampling; reconsider if (a) misses
      AC-8's budget.
- [x] **What do the scenes do during a transition?** Options: (a) **nobody updates by default; options for the outgoing
      scene during out and the incoming one during in**; (b) both keep updating; (c) no options. — **recommended: (a)**
      (FR-10). Freezing is what games expect behind a cover (no fall into a pit while the screen is black), is
      deterministic and makes input irrelevant; the options cover intros that play while the iris opens.
- [x] **How is input kept away?** Options: (a) **a block in the input module during the transition (queries idle, no
      input or action events) with "held controls stay blocked until released" at the end, on by default, an opt-out per
      transition**; (b) only by not updating scenes (event subscribers still see presses); (c) the game ignores input
      by querying `active()`. — **recommended: (a)** (FR-12). (b) still delivers `ButtonPressed` to subscribers (the
      platformer's `Demos` quits on START from an event) and leaks the held confirm press into the new scene; (c) is the
      bug every game re-writes. The block is also a small public input feature (cut-scenes), which the plan exposes or
      keeps internal.
- [x] **Where in the draw order is the overlay?** Options: (a) **recorded after every scene's `draw()`, last on layer
      15, ignoring the camera; game code after `update_and_draw()` draws above it**; (b) a reserved engine layer above
      15 in `gfx`. — **recommended: (a)** (FR-7). No graphics change; the debug pass stays on top either way, and the
      "draw after" escape hatch is useful (a HUD over a transition). (b) only if the plan finds a game case (a) can't
      serve.
- [x] **Loading.** Options: (a) **a per-frame loading step while covered (progress + done) and a minimum hold, plus the
      loading-scene pattern with no extra API**; (b) only a loading scene; (c) only synchronous loading in `on_enter()`. —
      **recommended: (a)** (FR-13, FR-14). Small loads need nothing (`on_enter()` behind the cover, FR-11), medium ones a
      bar on the cover, big ones a scene of their own; all three ride on the same guarantees. No threads (non-goals).
- [x] **Built-in loading indicator?** — **recommended: none; the custom effect receives the progress and the example
      draws a bar** (FR-8, non-goals). The engine forces no look (XI).
- [x] **Notifications: events or queries?** — **recommended: queries, on-covered and on-finished callbacks, and an
      optional finish event of the game's own type; no engine events** (FR-15, FR-16). It matches spec 015's
      `.publish` and specs 020/021's "no lifecycle events"; the stack change itself is already observable through
      `on_exit()`/`on_enter()`.
- [x] **Music.** Options: (a) **no audio dependency: callbacks and the duration queries let the game fade or crossfade,
      documented as patterns**; (b) a `.music` option calling the audio module. — **recommended: (a)** (FR-17). (b)
      would link the audio module (with Ogg, +60 KB on the 3DS) into every game using transitions (XI).
- [x] **Easings.** Options: (a) **spec 015's libm-free curves, moved to a light header if needed so the transition
      header doesn't pull the tween set API**; (b) linear only; (c) only the game's own curve function. —
      **recommended: (a)** (FR-3, FR-20, Constraints). Same bits everywhere and the names games already use; the move,
      if needed, keeps `<nnh/tween.hpp>` source-compatible (as D31 did for the frame clock).
- [x] **Screens.** — **recommended: one timeline for both screens; per screen the same effect, its own effect and
      colour, or none; an option to draw directional effects and the iris across both screens as one tall area with
      spec 018's layout (40-pixel inset, a gap setting)** (FR-6). Different timelines per screen are a non-goal; the tall
      area matches games using `CameraLayout::TallWorld`.
- [x] **A request while a transition runs.** Options: (a) **refused, returns false, a debug-level log**; (b) queued; (c)
      assert. — **recommended: (a)** (FR-19). A door overlapped for two frames or a button mashed on a menu is normal
      play, so (c) would crash games; (b) hides the double request and lengthens the transition.
- [x] **Plain stack changes during a transition.** — **recommended: allowed and applied as usual; the transition's
      change applies to the stack as it is at full cover** (FR-19). Asserting would forbid a scene popping a dialog of
      its own while frozen-but-alive callbacks run; the rule is simple to document.
- [x] **Draw capacity.** Options: (a) **overlay draws count against `max_draws`, with each effect's maximum documented
      (≤ 128 per screen at defaults)**; (b) a reserved overlay capacity. — **recommended: (a)** (FR-21). The default
      2048 leaves room; reserving capacity would cost every game memory for a few frames' use.
- [x] **Duration limits** — **recommended: 0 to 10 s for out, hold and in each; zero allowed** (FR-2, FR-3). Longer
      covers are loading steps (unbounded), not durations.
- [x] **Per-scene default transitions?** — **recommended: no; a game keeps its options as a constant** (non-goals).
      HaxeFlixel's global defaults and Monarch's per-screen handlers are hidden configuration; one `constexpr` options
      value per kind of change is as short and explicit.
- [x] **Example: a new `examples/transitions`, `examples/scenes` converted, or only the platformer?** — **recommended: a
      new `examples/transitions` plus the platformer's doors; `examples/scenes` keeps its instant changes** (AC-5, AC-7,
      AC-9). The new example shows every effect, screens, loading and music and owns the console self-check and bench;
      `examples/scenes` stays the "plain stack" reference (XI).
- [x] **Platformer: what happens to the replay checksum?** — **recommended: unchanged; doors get the iris, the replay's
      level switch and the demo cycling stay instant** (AC-7). Routing the replay's switch through a transition would
      add frozen frames and change every golden line after it for no gain in coverage; the doors are not on the replay's
      path (level 1's door is far to the right of where the replay ends).
- [x] **Old 3DS cost targets** — **recommended: AC-8's targets, confirmed by the plan, and set from the first console
      measurement with margin if they are still missed after optimisation** (the maintainer's bench policy).

## Open questions

- [ ] Should the input block (FR-12) be a public input API for games (cut-scenes, dialogs) in this spec, or internal to
      the transition module until a game needs it? — recommended: public, small (block, unblock, query), documented in
      `docs/api/input.md`; it is the same code and testable on its own.
- [ ] Should the fade use citro2d's `C2D_Fade` on the 3DS instead of a full-screen rectangle? — recommended: a rectangle
      on every system (the same submitted data on the fake platform), `C2D_Fade` only if the plan measures a real saving
      and the screenshots match.
- [ ] Does the iris centre accept a world point (converted with the screen's camera at the start of the transition) or
      only a screen point? — recommended: screen point only; games convert with `CameraRig::to_screen` or their own
      camera maths, keeping the module free of camera helpers (XI).
