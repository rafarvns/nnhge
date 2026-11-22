# Spec 016: Sprite animation

Status: implemented (console check pending, T10.2) — approved (2026-11-22, maintainer pre-approved recommendations: every open question below is answered with
the recommended answer)
Author: NNHGE maintainers · Date: 2026-11-22
Depends on: [spec 002](../002-core-foundation/spec.md) (`Duration`, memory tags, asserts, logs, `Result`, profiling),
[spec 004](../004-2d-graphics/spec.md) (`Texture`, `Region`, `gfx::draw` and its `DrawOptions`),
[spec 007](../007-assets-and-romfs/spec.md) (assets by path) and, for its clock type only,
[spec 015](../015-tweens-and-timers/spec.md) (`FrameTime`/`Frames`, which this spec asks to move to the foundation:
see the open questions). Its Aseprite stage extends `nnh-assetc` ([D27](../../docs/architecture/decisions.md)), the
host tool of [spec 012](../012-tilemaps-and-collision/spec.md). Verified on the console with
[spec 013](../013-console-remote/spec.md)'s `nnh run --script`. Implements the ROADMAP item "Sprite animation" of
"Features in the spirit of Godot and Unity". Optional for games (constitution XI, decision D25).

## Problem and goal

Almost every sprite in a game is animated: a hero that idles, runs, jumps and falls, a coin that spins, an enemy
whose attack hits on its fourth frame, a footstep sound on the frames where a foot touches the ground. Today each
game writes it by hand — the platformer example picks `frame = 1 + (walk_frames_ / 8) % 2` — and every game repeats
the same counters, the same "which frame is it now", the same `if (on_ground) ... else ...` to choose the animation,
and the same off-by-one bugs when a clip should play once, hold its last frame or bounce back and forth. Godot
(`AnimatedSprite2D` + `SpriteFrames`) and Unity (`Animator`, animation events) make this declarative. After this
spec a game describes its sprite sheet and its named clips once (in code, or by exporting them from Aseprite), gives
each moving thing a small **animator** value, calls `play("run")` or lets a tiny state machine pick idle/run/jump,
and draws the animator's current frame with `gfx::draw`; frame events ("step", "hit") reach the game on the exact
frame, with no allocation while playing and the same frames on PC and 3DS, frame for frame. A game that prefers its
own counters ignores all of it and pays nothing.

## User stories

- As a game developer, I want to describe a sprite sheet as a grid of equal frames, or as a list of rectangles, so
  that I name frames by index instead of computing pixel regions.
- As a game developer, I want named clips (`idle`, `run`, `attack`) made of frames with a default speed and optional
  per-frame durations, that loop, play once and hold, play a number of times, or ping-pong, so that timing lives in
  one place instead of in my update code.
- As a game developer, I want a small animator per sprite that plays a clip, queues the next one, changes speed,
  pauses, flips, and tells me the current frame's region and pivot, so that drawing an animated sprite is one
  `gfx::draw` call.
- As a game developer, I want events on frames (a footstep, the frame where a sword hits) delivered exactly once per
  frame entered, even when several frames pass in one update, so that sounds and hit boxes line up with the art.
- As a game developer, I want a small state machine (idle, run, jump, fall, attack) where I say which state I want
  each frame and the machine decides whether it may switch now, at the end of the clip, or never, and moves on by
  itself when a one-shot clip ends, so that "attack can't be interrupted" and "jump goes to fall" are data, not
  flags.
- As a game developer who draws in Aseprite, I want to export my sprite (PNG + JSON with tags) and have the build
  turn it into clips I load by path, so that changing timings or adding frames needs no code change.
- As a game developer, I want 100 enemies to share one set of clips and each keep only a small animator, with no
  allocation while they play, so that animation costs almost nothing on an Old 3DS.
- As a game developer, I want animation to advance by game frames, the same on every machine, so that replays and
  console tests stay exact.
- As a game developer who prefers my own code, I want to use none of it, or only the sheet (frame regions) without
  clips, and lose nothing.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### Sprite sheets and frames

- **FR-1 Frames of a sheet.** A game describes the frames of a sprite sheet either as a **grid** (frame width and
  height, number of columns or the image width, frame count, optional margin and spacing, read row by row) or as an
  **explicit list of regions** (`Region{x, y, w, h}` in texture pixels). Each frame has a region and a **pivot**
  (default: the clip library's default pivot, e.g. bottom-centre, or top-left if none is given) and, for trimmed
  sheets, an **offset** so that a trimmed frame is drawn exactly where the untrimmed frame would be. Frames are
  referred to by index (0-based, in declaration order). The sheet describes geometry only: it does not load, own or
  name a texture; the game draws with its own `Texture` (FR-10).
- **FR-2 Validation.** Building frames checks every value and returns an error (never asserts on data): non-positive
  sizes, a grid that doesn't fit the given image size, more frames than the capacity, a region with negative or zero
  size or (when the image size is given) outside the image. The error names the frame.

### Clips and the clip library

- **FR-3 Clips.** A **clip** has a name (unique within its library, ≤ 31 bytes of UTF-8), an ordered list of frame
  indices (1–256; a frame may appear several times), a **default frame duration**, optional **per-frame durations**
  (overriding the default), a **playback mode** and a **direction**:
  - modes: **loop** (forever), **once** (plays once and holds its last shown frame), **repeat n** (n ≥ 1 complete
    plays, then holds), **ping-pong** (forward then backward, see FR-4) forever or n times;
  - directions: **forward** or **reverse** (Aseprite's `reverse`; ping-pong reverse = Aseprite's
    `pingpong_reverse`).
  Durations are given as a `Duration` or as a whole number of 60 Hz frames, on the same frame clock as spec 015
  (FR-14). A zero or negative duration is an error (FR-2's rule).
- **FR-4 Exact sequences.** The order of shown frames is defined and tested for every mode, with the turning frames of
  ping-pong never shown twice in a row: frames `0 1 2 3` in ping-pong show `0 1 2 3 2 1 0 1 2 3 2 1 …`; one ping-pong
  play is `0 1 2 3 2 1` and a finite ping-pong ends by showing the first frame (`… 2 1 0`) and holding it; reverse
  starts from the last frame; a one-frame clip shows its frame and, in once/repeat mode, finishes after its duration.
  Aseprite plays tags the same way (FR-17 converts them without changing the result).
- **FR-5 Clip library.** Frames and clips live in a **clip library** that the game creates (in code) or loads (FR-16)
  and owns. A library has **fixed capacities** given at creation (frames, clips, frames over all clips, markers,
  state-machine states and transitions; FR-12); its storage is **one block reserved at creation** (memory tag
  `animation`, visible in the leak report and memory overlay) and released when the library is destroyed. Adding to
  a full library returns an error. Clips are found by name (`find_clip("run")`, returning a small **clip id**) and
  may be played by id (no lookup) or by name (a lookup; documented cost). A library is **read-only while animators
  use it**: any number of animators share one library; the docs state the lifetime rule (the library must outlive
  the animators that use it), and Debug/Development builds catch a use after the library is destroyed loudly
  (assert), Release pays nothing (constitution X).

### The animator

- **FR-6 Animator.** An **animator** is a small plain value (no heap, no `virtual`; its size is documented, target
  ≤ 64 bytes on 3DS) that plays clips of one library. Commands: `play(clip)` (starts it from its first frame, or does
  nothing if that clip is already playing unless the game asks to restart), `play(clip, start_frame)`, `queue(clip)`
  (plays after the current clip finishes; for a looping clip, at the end of its current loop; a small fixed queue,
  ≤ 4 entries; a full queue reports an error and keeps the queue), `stop()` (no clip), `pause()`/`resume()`,
  `set_frame(i)` (jumps within the current clip), and a **speed** (1 by default; 0 freezes; e.g. 1.5 for a faster
  run; negative is rejected — reverse is a clip direction) applied with exact arithmetic (no drift).
  Queries: playing/paused/finished, the current clip id and name, the frame position within the clip, the **sheet
  frame index**, its **region**, **pivot** and **offset**, the time spent in the current frame, progress of the clip
  (0–1), loops completed.
- **FR-7 Advancing.** An animator advances only when the game calls its `update()` (exactly one 60 Hz frame) or
  `advance(FrameTime)` (any span: driven by `time::FixedStep`, a skip, or — the game's choice, not deterministic —
  `time::delta()`). The time left over at the end of a frame carries into the next frame and the next loop, so a
  clip never drifts: a clip of four 6-frame frames, looped 10 times, ends its tenth loop exactly on update 240. An
  advance longer than several frames passes through every frame in order (FR-8 sees each of them), with a documented
  upper bound on frames crossed per call (beyond it: assert in Debug/Development, error log in Release and the
  animator jumps to the right frame without the skipped markers).
- **FR-8 Frame markers (events on frames).** A library can attach **markers** to frames of a clip: a name (hashed for
  comparison, ≤ 31 bytes) and an optional small integer value (`"step"`, `"hit" 2`). Whenever an animator **enters**
  a marked frame (by playing, looping, ping-pong turning, `set_frame` or a state change), the marker fires **once**,
  in frame order, **during the animator's next `update()`/`advance()`** (so markers never fire from inside `play()`
  and always fire at a single, predictable point). Alongside markers the animator reports **clip finished** (once /
  repeat n / finite ping-pong), **looped** and **state changed** (FR-10) as notifications of the same kind. The game
  receives notifications through
  - an optional **callback** on the animator, stored inline with no allocation (the same size rule as spec 015's
    callbacks, today 16 bytes: `this` plus one pointer or id), called for each notification in order; the game
    publishes its own events from it if it wants (`events::publish(Footstep{id})`); and
  - a **query** of the notifications produced by the last update (a small fixed list, bounded as in FR-7), for code
    that prefers polling.
  The engine publishes **no** generic animation event on the bus (one event per meaning,
  [events.md](../../docs/architecture/events.md)).
- **FR-9 Re-entrancy.** A callback may call any command of the same animator (play, queue, stop, a state request).
  The change applies at once; notifications still pending from the old clip in that update are dropped; a chain of
  callbacks that keeps changing clips within one update is stopped by a documented depth limit with a logged error.
  Another animator's commands may be called freely.

### Drawing

- **FR-10 Drawing the current frame.** The animator gives what `gfx::draw` needs: the current region and the pivot
  (adjusted by the frame's offset, FR-1, and mirrored correctly when flipped). It carries **flip_x/flip_y** flags
  (state the game sets, e.g. facing left) and a helper fills a `gfx::DrawOptions` (region, pivot, flips) the game can
  then complete (layer, tint, opacity) and pass to `gfx::draw` with its texture. With a trimmed frame, a flipped
  sprite and an untrimmed one land on the same pixels (tested). Drawing adds no cost beyond one `gfx::draw` call.
- **FR-11 Sheets without clips.** A game can use a library's frames alone (region and pivot of frame *i*) without
  clips or animators, for static sprite atlases.

### State machine

- **FR-12 States.** A library can declare a small **state machine**: named **states**, each playing one clip, with an
  optional **next state on finish** (a once/repeat clip that ends moves on by itself: `jump` → `fall`, `attack` →
  `idle`). An animator using the machine starts in a given state.
- **FR-13 Requests and transitions.** The game drives the machine by **requesting** a state, normally every update
  from its own conditions (`request(on_ground ? (moving ? Run : Idle) : (vy < 0 ? Jump : Fall))`). Requesting the
  current state does nothing (the clip doesn't restart). Whether a request is honoured is decided by a
  **transition table** of rules (from a state or *any* state, to a state or *any* state):
  - **immediately** (default when no rule matches),
  - **at the end** of the current clip or of its current loop (the request is remembered; a later request replaces
    it), or
  - **never** (the request is ignored: an attack that can't be cut short except by `hurt`);
  a rule may also **keep the time** of the current clip (switching `run` ↔ `run_shoot` on the same frame). The most
  specific rule wins (from-to, then from-any, then any-to, then any-any), documented and tested. A game may also
  **force** a state (bypassing the rules, e.g. on death). Each state change is a notification (FR-8) and is queryable
  (current state, previous state, time in state). There are no parameters, conditions evaluated by the engine,
  blend trees or layers (non-goals): the game's code is the condition.
- **FR-14 Deterministic.** Given the same library, the same calls in the same order and the same number of updates,
  an animator shows the same frames and produces the same notifications in the same order, bit for bit, on PC and
  3DS. Durations use spec 015's frame clock (ticks of 1/4096 frame; 1 s = 60 frames exactly; a `Duration` is rounded
  once when the clip is created or converted, with the rounding documented) and integer arithmetic only.

### Clock, ownership and cost

- **FR-15 Game-owned, no hidden state.** Libraries and animators are owned by the game (normally a library per kind
  of sprite, loaded or built in a scene's `on_enter` or the game's start, and an animator member per entity). The
  engine keeps no global library or animator list, updates nothing by itself and needs no scene binding: an animator
  advances only inside the game's own update (FR-7), so no callback can run after its scene left the stack. Updating
  an animator costs time proportional to the frames crossed (constant in the normal case of zero or one), with no
  allocation, no `virtual` call and no logging in the normal path; a profile scope covers loading and the bench
  (per-animator scopes would cost more than the work).

### Aseprite

- **FR-16 Loading clip libraries.** The game loads a converted library by asset path (`load_animation("art/hero")` or
  the name the plan fixes), getting the same kind of library as one built in code, sized exactly for its contents
  plus optional extra capacity the game asks for. Loading validates the whole file with a bounded validator shared
  with the build (constitution X: the console treats the file as untrusted), returns errors (`Result`) and is
  fuzz-tested.
- **FR-17 Aseprite export converted at build time.** A file in the assets folder exported by Aseprite as a sprite
  sheet's data (`File → Export Sprite Sheet` or the CLI's `--data`, the JSON next to its PNG) and named by the
  convention the open questions fix (`<name>.ase.json`) is converted by `nnh-assetc` (D27: one step per file, a
  depfile, reconverted only when it changes) into a clip library asset at `<name>`, while the PNG becomes the texture
  at the same path (so `load_texture("art/hero")` and `load_animation("art/hero")` go together). Supported:
  - both JSON layouts: **Hash** (frames keyed by name; order = file order) and **Array**;
  - per frame: `frame` (region), `duration` (ms → frame clock, FR-14), `trimmed`/`spriteSourceSize`/`sourceSize`
    (→ FR-1's offset); `rotated: true` is rejected (Aseprite never writes it; other packers do);
  - `meta.frameTags` → clips: `name`, `from`, `to`, `direction` (`forward`, `reverse`, `pingpong`,
    `pingpong_reverse`), and `repeat` when present (→ repeat n / finite ping-pong; absent → loop);
  - frame markers (FR-8): a tag whose name starts with `@` (`@step`) is not a clip but puts the marker `step` on every
    frame it covers, in every clip that shows those frames;
  - `meta.slices`: a slice named `pivot` (its first key's `pivot`, or its rectangle's centre-bottom) gives the default
    pivot; other slices are ignored;
  - the JSON's `meta.image` must name the PNG beside it; `meta.size` must match it.
  Every problem stops the build with `file:line: reason` (unknown direction, a tag out of range, duplicate clip names,
  durations out of range, over a capacity ceiling, a missing PNG), like spec 012's Tiled converter. Sprites with no
  tags produce one clip, `default`, with all frames in order.
- **FR-18 State machines and markers in code on loaded libraries.** A game can add (within extra capacity requested
  at load) markers, states and transition rules to a loaded library in code before its animators start, because
  Aseprite has no notion of them beyond FR-17's `@` tags. Clips built in code and clips loaded from Aseprite behave
  identically.

### Opt-in

- **FR-19 Optional and independent.** Sprite animation is its own optional module. A game that doesn't use it links
  no animation code (checked on executable size) and pays nothing in scenes or graphics. It needs no other optional
  feature: not tweens (spec 015), not tilemaps (whose own animated tiles stay as they are), not data assets (spec 014);
  none of them needs it. The Aseprite converter is built only when a game has Aseprite files (as D27 does for maps).

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: grid frames (margin, spacing, columns, image
  size) and explicit regions, every FR-2 error; clips and their errors; the exact frame sequences of FR-4 for loop,
  once, repeat n, ping-pong forever and n, reverse, ping-pong reverse, one-frame and repeated-frame clips; per-frame
  durations in frames and `Duration`; play (same clip no restart, restart, start frame), queue (once, looping, full),
  stop, pause, resume, set_frame, speed (0, 0.5, 1.5 exact); markers fired once per entered frame in order, including
  several frames crossed in one advance, on loop and ping-pong turns, after play (on the next update) and the FR-7
  bound; callback and query see the same notifications; re-entrancy (FR-9, depth limit); regions, pivots, offsets and
  flips (FR-10, trimmed vs untrimmed land on the same pixels); state machine: requests, the four rule kinds and
  their priority, keep-time, next-on-finish, force, notifications; library capacity errors; Debug detection of a
  destroyed library.
- **AC-2 Frame exactness.** Tests show: a clip of 4 frames × 6 frames loops exactly every 24 updates and ends loop 10
  on update 240; 100 ms per frame (Aseprite) equals 6 updates; speed 0.5 doubles the counts exactly; a `Duration`
  that is not a whole number of frames never drifts over 1 000 loops.
- **AC-3 No allocation while playing.** A test counts allocations: after a library is created or loaded, playing,
  queueing, updating, firing markers and switching states on many animators allocate nothing; the leak report is
  clean after libraries are destroyed.
- **AC-4 Aseprite conversion.** `nnh-assetc` tests with fixture exports (Hash and Array, trimmed and untrimmed, every
  direction, `repeat`, `@` marker tags, a `pivot` slice, no tags) produce the expected libraries; malformed fixtures
  fail with `file:line: reason`; the core validator rejects corrupted converted files; both the converter's JSON input
  and the loaded binary have fuzz targets (mini-fuzz in `nnh_tests`, libFuzzer on Linux), like spec 012's. Editing
  one export reconverts only that file.
- **AC-5 Example `examples/animation`** (original art generated by a script, as the platformer's; its Aseprite
  export is a JSON written in Aseprite's format by the same script, so no Aseprite licence is needed to build it):
  top screen a gallery of clips (loop, once, repeat, ping-pong, reverse, a speed slider) with their frame counters;
  bottom screen a character driven by the state machine (idle, run, jump, fall, attack that can't be interrupted,
  hurt that can) whose footsteps play a sound and flash a marker from frame markers, with one library built in code
  and one loaded from the Aseprite export. PC screenshot (`NNH_EXIT_AFTER_FRAMES` + `NNH_SCREENSHOT`, deterministic
  frame) checked by reading the PNG; a run on an Old 3DS at 60 Hz with the debug overlay.
- **AC-6 Determinism on the console.** The example has a self-check mode that plays a fixed script of clips, queues,
  speeds, state requests and markers on many animators for a fixed number of frames, logs a checksum of shown frames
  and of the notification order every 60 frames, and ends with `animation: PASS <checksum>` or
  `animation: FAIL ...` against golden values recorded from the simulator. On an Old 3DS the maintainer only starts
  the Homebrew Launcher and presses Y; `nnh run animation --script <file>` (spec 013) waits for the PASS line and
  fails on a FAIL line; the same script passes with `--pc`. The verdict is in the saved log.
- **AC-7 Cost on the console.** Measured on an Old 3DS with the profiler in the example's bench mode: updating 512
  animators (mixed clips, markers and state machines) stays under 0.5 ms per frame (target; the plan confirms it); the
  number goes in the docs.
- **AC-8 The platformer uses it.** `examples/platformer`'s player draws through an animator with idle, run, jump and
  fall clips chosen by a state machine (replacing its hand-written frame choice), with a footstep marker; its unit
  tests, its replay test (`platformer_replay_matches_script`), `gate.nnhs` and `level.nnhs` pass unchanged (animation
  affects drawing only). Screenshot of the player running checked by reading the PNG.
- **AC-9 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS; examples that don't
  use animation are unchanged; a 3DS build with no Aseprite files never builds the converter.
- **AC-10 Docs.** `docs/api/animation.md` (sheets, clips and modes with the frame sequences of FR-4, animator,
  markers and notifications, drawing, state machine with its rule table, the clock, capacities, sizes and costs,
  the lifetime rule), the Aseprite workflow in `docs/api/assets.md` (or a guide: export settings, naming, what is
  read, errors), links from `docs/api/gfx.md`, a decision row in `docs/architecture/decisions.md` if the plan adds
  one, CLAUDE.md layout and ROADMAP updated, `docs/REFERENCES.md` citing Godot `AnimatedSprite2D`/`SpriteFrames`,
  Unity's Animator and animation events, and Aseprite's sprite sheet and CLI docs.

## Non-goals

- An editor or preview tool (constitution XI); Aseprite (or code) is the editor.
- Reading `.aseprite`/`.ase` binaries directly or running Aseprite during the build: Aseprite is a paid tool not
  every contributor has, and its binary format is a larger parser surface. The JSON + PNG export is the input.
- Aseprite layers as separate sprites, cel user data, tag colours and user data, linked cels, `json` sheets from other
  packers (TexturePacker, Free Texture Packer) unless they happen to match the supported subset; rotated frames.
- Clips declared in data assets (spec 014's TOML files): a natural follow-up once both exist; this spec fixes the
  library's binary format so a later converter can target it.
- A Unity-style parameter system (bools, triggers, floats evaluated by the engine), blend trees, layers, sub-state
  machines, cross-fades between clips: the game's code is the condition (FR-13).
- Animating anything other than which frame is shown (position, rotation, colour per frame — use tweens, spec 015;
  hit boxes per frame — use markers or the game's own table per frame index).
- Engine-owned or scene-bound automatic updates of animators (an "animator group" bound to a scene through D30): not
  needed, since animators run inside the game's update; a follow-up if games ask.
- Skeletal/cut-out animation, 3D animation, palette swaps.
- Tile animations: spec 012 already has them, with their own clock.
- Threads: animators and libraries are used from the game's thread only.

## Constraints

- **I** — the same calls, the same frames on PC and 3DS; portable core code with no platform part; the converter is a
  host tool.
- **II** — fixed capacities chosen by the game, one block per library (not on the 3DS main thread's 32 KB stack),
  small animator values, bounded per-frame cost (AC-7).
- **III** — the public header stays light (no `<functional>`, no `<string>`); building `examples/hello` is
  unaffected; an Aseprite edit reconverts only that file (D27).
- **IV** — state (current frame, state) is queried; notifications are delivered inside the owner's update; the game
  decides whether to publish its own events. No engine event per frame.
- **V** — no exceptions, no RTTI, no `virtual` per animator, no allocation while playing (AC-3), callbacks inline.
- **VI** — pure logic: every behaviour unit-tested first on the fake platform; the example by screenshot and on the
  console.
- **VII** — frames, clips, the library, the animator, markers, the state machine, loading and the converter in small
  separate files; one public header.
- **VIII** — header comments, `docs/api/animation.md`, `examples/animation`, the platformer change.
- **IX** — Aseprite's JSON format is documented and its export is the user's own work; the converter is written from
  the documented format; no code copied from incompatibly licensed loaders; example art is original.
- **X** — the converted file is untrusted on the console: bounded validator, fuzzed (with the converter's JSON input);
  misuse asserts in Debug/Development and logs in Release; data errors are returned, never asserted.
- **XI, D25** — entirely optional; frames work without clips, clips without the state machine, code without Aseprite;
  hand-written frame counters stay fully supported.
- **Under pressure:**
  - **XI and the shared clock.** Spec 015 defines `FrameTime`/`Frames` in `<nnh/tween.hpp>`. Using them from the
    animation module would make animation depend on the tween module, which XI forbids. Recommended (open question
    below): move `Frames` and `FrameTime` to the foundation's `<nnh/time.hpp>` (plan 015 is approved but not
    started, so this is a small amendment there, with no game-code change: `<nnh/tween.hpp>` includes `time.hpp`).
  - **Spec 007's asset naming.** A `.json` in the assets folder is today a plain data file; FR-17 reserves the
    `.ase.json` suffix for Aseprite exports. Existing games are unaffected (no example uses that suffix); the plan
    records it in `docs/api/assets.md` and, if it judges it an architecture rule, a decision row.
  - **D27 grows a second converter** (Aseprite) and a second fuzzed input format; the plan keeps it a separate
    `nnh-assetc` command with its own files, built only when needed.

## Prior art

- Godot `AnimatedSprite2D` and `SpriteFrames`
  ([AnimatedSprite2D](https://docs.godotengine.org/en/stable/classes/class_animatedsprite2d.html),
  [SpriteFrames](https://docs.godotengine.org/en/stable/classes/class_spriteframes.html)): named animations with a
  speed (FPS) and relative per-frame durations, loop flag, `play`/`play_backwards`/`pause`/`stop`, `speed_scale`,
  signals `frame_changed`, `animation_finished`, `animation_looped`. FR-3, FR-6 and FR-8's notifications follow it;
  `SpriteFrames` shared by many sprites is FR-5's library.
- Unity `Animator` and animation events
  ([Animator](https://docs.unity3d.com/Manual/class-Animator.html),
  [transitions](https://docs.unity3d.com/Manual/class-Transition.html),
  [animation events](https://docs.unity3d.com/Manual/script-AnimationWindowEvent.html)): states → clips, transitions
  with "has exit time", any-state transitions; events at a time in a clip calling a function. FR-13's "at the end"
  rule is "has exit time"; "any" rules are Any State; FR-8's markers are animation events; Unity's parameters are a
  non-goal.
- Aseprite sprite sheet export ([sprite sheet](https://www.aseprite.org/docs/sprite-sheet/),
  [CLI](https://www.aseprite.org/docs/cli/), `--format json-hash|json-array`, `--list-tags`, `--list-slices`):
  `frames` with `frame`, `duration`, trim data; `meta.frameTags` with `direction` (`forward`, `reverse`, `pingpong`,
  `pingpong_reverse`) and, in recent versions, `repeat`; `meta.slices` with pivots. Other loaders handle the same
  subset, e.g. [Brine2D's Aseprite integration](https://www.brine2d.com/animation/aseprite.html) (tag repeat →
  repeat count, `pingpong_reverse` → reversed ping-pong) and the `pingpong_reverse` order fixed in
  [sgl PR #407](https://github.com/stevepryde/sgl/pull/407) (`3 2 1 0 1 2`, ends never doubled). The plan checks
  FR-4's sequences and the type of `repeat` (number or string) against a real Aseprite export before freezing the
  converter.
- Spec 012's animated tiles (`Tilemap::set_tile_animation`, durations in ms, Tiled's format) and spec 015's frame clock
  and inline callbacks: same ideas, reused rather than redefined.

## Open questions

All answered with the recommended answer, pre-approved by the maintainer (2026-11-22).

- [x] **Include Aseprite in this spec or a later one?** — **answer: in this spec, as its last stage** (FR-16–FR-18).
      The ROADMAP item names it; `nnh-assetc` (D27) already has the JSON reading, depfiles, `file:line` errors and
      fuzzing set up for Tiled; and the library's loaded format is the same one code-built libraries use, so the core
      stages ship and are useful on their own before it. If the stage slips, the code-only stages stand alone.
- [x] **Which clock?** — **answer: spec 015's frame clock** (`FrameTime` ticks of 1/4096 frame, `Frames`), not a new
      one (FR-14). Animators advance by game frames like tween sets: deterministic, no drift, replays stay exact;
      `advance(FrameTime)` covers FixedStep and deliberate wall time.
- [x] **Where does `FrameTime` live?** — **answer: move `Frames`/`FrameTime` to `<nnh/time.hpp>`** (foundation) as an
      amendment to plan 015 before it starts, so animation doesn't depend on tweens (XI). If plan 015 is implemented
      first as written, the move happens in this spec's first stage; game code is unchanged either way.
- [x] **Frame events: published on the bus or callbacks?** — **answer: an inline callback on the animator plus a
      query of the last update's notifications; no engine bus event** (FR-8). Markers are data (from Aseprite, too),
      so they can't be the game's typed events; a generic `AnimationMarker` event would be the "generic event"
      anti-pattern and arrive a frame late. The callback runs at the exact frame inside the game's update and can
      publish the game's own event in one line.
- [x] **When do markers of a frame entered by `play()` fire?** — **answer: at the animator's next update**, before
      time advances (FR-8). Notifications then only ever happen inside `update()`/`advance()`, which keeps
      re-entrancy simple and the order deterministic.
- [x] **Ping-pong semantics** — **answer: Aseprite's** (FR-4): turning frames are not doubled; one play is
      `0 1 2 3 2 1`; a finite ping-pong ends on the first frame. Converted Aseprite tags look exactly as in Aseprite.
- [x] **State machine scope** — **answer: states → clips, next-on-finish, game requests every frame, a rule table
      (immediately / at end / never, keep time) with any-state wildcards, and force** (FR-12, FR-13). No engine-side
      parameters or conditions: the game's own `if` is the condition, which is shorter in C++ than declaring
      parameters and stays testable. This covers idle/run/jump/fall/attack/hurt, the ROADMAP's example.
- [x] **Is the state machine part of the library or separate?** — **answer: part of the library** (shared, read-only
      like clips, within the library's capacities); the animator holds the current and pending state. One object to
      create and load; many enemies share one machine.
- [x] **Scene binding (D30) for animators?** — **answer: no** (non-goals). Animators are updated by the entity that
      owns them, in the order the game's logic needs (after deciding the state, before drawing); callbacks then run
      only inside the game's update, so nothing can call back into a scene that left. D30 stays available for a
      later "animator group" if games ask.
- [x] **Who owns the texture?** — **answer: the game** (FR-1, FR-10). The library holds geometry and timing only; the
      game loads the texture with `load_texture` (for Aseprite exports: the same path) and passes it to `gfx::draw`.
      One library can serve palette-swapped copies of a sheet, and the module needs no graphics resources.
- [x] **Names: strings or ids?** — **answer: names at creation and load (≤ 31 bytes, kept for logs and the debug
      overlay), hashed to 32 bits; lookups return a clip/state id the game keeps** (FR-5). Duplicate names (or hash
      collisions) are errors at creation and in the converter. Playing by id costs nothing; by name, a short linear
      lookup (documented).
- [x] **Capacity ceilings and sizes** — **answer:** capacities are required at creation (no hidden default); ceilings
      per library: ≤ 4096 frames, ≤ 256 clips, ≤ 256 frames per clip, ≤ 8192 clip frames in total, ≤ 1024 markers,
      ≤ 32 states, ≤ 128 transition rules; animator target ≤ 64 bytes on 3DS with a 4-entry queue; ≤ 32
      notifications per update (FR-7's crossing bound derives from it); callback captures ≤ 16 bytes; re-entrancy
      depth 8 (as `events::kMaxDepth`). The plan sizes each record and states the memory per entry.
- [x] **Header and module name** — **answer: `<nnh/animation.hpp>`, module `animation`, memory tag `animation`**,
      matching D25's list ("animation"). Types are prefixed so a later skeletal or property animation feature can
      live beside them without renaming (exact names are the plan's).
- [x] **Aseprite file convention** — **answer: `<name>.ase.json` beside `<name>.png`** in the assets folder, giving
      the texture and the clip library at `<name>`. Detecting Aseprite by sniffing every `.json` would turn the
      game's own JSON data files into build errors; a suffix is explicit, documented in the export instructions
      (`--data hero.ase.json`) and costs nothing to existing games.
- [x] **Markers from Aseprite** — **answer: tags whose name starts with `@`** (FR-17). Aseprite has no frame events;
      tags are visible on its timeline, survive every export option and can overlap clip tags. Tag user data is
      ignored (its export varies by version); markers and states can also be added in code (FR-18).
- [x] **Example: new `examples/animation`, the platformer, or both?** — **answer: both** (AC-5, AC-6, AC-8). The new
      example shows every mode, the Aseprite path and owns the console self-check; the platformer's player is the
      real-game proof (its hand-written frame choice is exactly what this replaces) and must keep its replay and
      scripts passing unchanged.
- [x] **Console check** — **answer: spec 013's script runner with a logged verdict** (AC-6): the maintainer only
      opens the Homebrew Launcher and presses Y; `nnh run animation --script` reads the `PASS`/`FAIL` line.
