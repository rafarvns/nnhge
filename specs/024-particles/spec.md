# Spec 024: Particles

Status: approved 2026-11-23 (overnight pre-approval by the maintainer: every "Decisions to take" item and open
question as recommended; to review in the morning). Implemented after spec 022 (it uses 022's transform value and sine)
Author: NNHGE maintainers · Date: 2026-11-23
Depends on: [spec 004](../004-2d-graphics/spec.md) (textures, `DrawOptions`, layers, the recorded draw list, `max_draws`)
and its tile batches ([spec 012](../012-tilemaps-and-collision/spec.md), decision D26; zoom D33,
[spec 018](../018-camera-helpers/spec.md)), [spec 002](../002-core-foundation/spec.md) (asserts, logs, `Result`, tagged
memory, profiling), [spec 015](../015-tweens-and-timers/spec.md)'s frame clock (`FrameTime`, `Frames` in
`<nnh/time.hpp>`) and [spec 022](../022-scene-tree-and-transforms/spec.md)'s transform value and public deterministic
sine and cosine (approved, not yet implemented; see Constraints). Works with [spec 016](../016-sprite-animation/spec.md)'s
sprite libraries (frames for particles), [spec 020](../020-entities/spec.md)'s worlds and spec 022's nodes (an emitter
follows an entity) and [spec 014](../014-data-assets/spec.md)'s data assets (emitter presets in text files) without
depending on any of them. Verified on the console with [spec 013](../013-console-remote/spec.md)'s `nnh run --script`.
Implements the ROADMAP item "Particles" (Features in the spirit of Godot and Unity). Optional for games (constitution
XI, decision D25).

## Problem and goal

Sparks when a coin is taken, dust when the player lands, smoke from a chimney, a muzzle flash, rain, an explosion: every
2D game needs many short-lived sprites that are born, move, fade and die. Today an NNHGE game writes this by hand — the
platformer's coin sparkles are prefab entities, each with its own velocity component and a fade tween — which costs an
entity, a tween and a `gfx::draw` record per sparkle, uses `sin`/`cos` and `rand` from the C library if it wants a
random spread (different bits on PC and 3DS, so replays drift), and gives no easy way to say "40 per second, from a
circle, upward ±30°, yellow to transparent red over 0.5 s, shrinking". Godot (`CPUParticles2D`/`GPUParticles2D`), Unity
(the Particle System's modules), LÖVE (`ParticleSystem`) and Phaser (`ParticleEmitter`) all give games an **emitter**
configured by a small set of values. After this spec an NNHGE game describes an emitter in one plain C++ struct (or a
text file through spec 014) — emission rate, bursts and one-shot bursts at any point, a shape, direction and speed
ranges, lifetime, gravity and drag, angular velocity, colour and size over lifetime, sprite frames — creates it in
**one block** with a fixed capacity, updates it once per frame and draws it with **one draw-list command per emitter**,
within a particle budget per frame that is separate from `max_draws`. The simulation is the **same bits on PC and
3DS** (seeded, libm-free), allocates nothing while playing, and a game that doesn't use particles pays nothing.

## User stories

- As a game developer, I want to describe "yellow sparks, 16 in a burst, from a small circle, upward ±60°, 60–120 px/s,
  gravity down, fading out over 0.4 s" in one struct, and spawn that burst wherever a coin is taken with one call, so
  that effects take minutes instead of a component, a tween and a loop each.
- As a game developer, I want a continuous emitter (smoke, a torch, rain) with a rate per second that starts, stops
  (letting the live particles finish) and restarts, so that ambient effects are a few lines.
- As a game developer, I want an emitter to follow a moving thing — a ship's exhaust, a node of the hierarchy, the
  player's feet — with its particles either left behind in the world (a trail) or carried along (a flame on a torch),
  so that I choose the look with one value.
- As a game developer, I want a fast emitter's trail to be a continuous line, not clumps at each frame's position, so
  that effects look smooth at 60 Hz.
- As a game developer, I want particles to change colour, opacity and size over their life, spin, and play frames of a
  sprite sheet (or a random frame each), so that one small texture gives many looks; and plain coloured squares when I
  have no art yet.
- As a game developer, I want particles drawn on a layer like any sprite, between my tile layers, following the camera
  and its zoom, or fixed on the screen for HUD effects, so that they sit where I put them.
- As a game developer, I want a hard, visible budget — a capacity per emitter and a particle count per frame — and
  particles that are dropped and counted, never a crash or a slowdown, when an effect asks for more, so that a big
  explosion can't break the frame rate.
- As a game developer, I want the same seed and the same calls to give the same particles bit for bit on PC and 3DS,
  so that replays and console checks still compare exactly, and an effect never changes another effect's randomness.
- As a game developer, I want to tune an effect in a text file and see it after a data-only rebuild, without compiling
  C++, so that iterating on the look is fast.
- As a game developer who draws my own effects (or uses pools and sprites), I want my game, its size and its build time
  to stay as they are.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### Emitters

- **FR-1 An emitter object.** An emitter is an object the game creates and owns (a scene member, a level member, a
  game-wide object), never in a hidden global registry. It is created by a `Result`-returning `create(desc, capacity)`
  in **one block** of tagged memory (tag chosen by the game, default an engine tag) holding its live particles, its
  description and its precomputed curves. Capacity is 1..8192 particles. Errors (`InvalidArgument` for a description
  out of range, `OutOfMemory`) are returned. An emitter is movable, not copyable. A game may have any number of
  emitters; one emitter serves many effects at different places (FR-6).
- **FR-2 A description struct.** An emitter is configured by one plain, trivially copyable **description** struct with
  designated-initialiser-friendly defaults (a default description is a valid emitter: white 1-pixel particles at a
  point, 1 s of life, no motion). It holds every value of FR-4 to FR-12. Ranges ("60–120 px/s") are a min and a max,
  each particle drawing uniformly between them (equal values = no randomness). Its members use only kinds spec 014 can
  load (`float`, integers, `bool`, enums, `Vec2`, `Color`, fixed arrays), so FR-21's presets need no other struct.
  `set_desc(desc)` replaces the description of a live emitter (curves recomputed; live particles keep their already
  drawn random values); it is a load-time-cost call (bounded, documented), not for every frame.
- **FR-3 Units.** Positions in pixels; velocities and gravity in pixels per second (and per second², applied per 60 Hz
  frame); angles in radians, clockwise on the y-down screen (as `DrawOptions::rotation` and spec 022); lifetimes,
  delays and intervals as `FrameTime` (spec 015's frame clock: a `Duration` or `Frames`). One `update()` is exactly
  one 60 Hz frame on every machine; nothing reads wall time.

### Emission

- **FR-4 Rate.** An emitter that is **emitting** creates particles at `rate` per second. The count per frame comes from
  a fixed-point accumulator on the frame clock, so fractional rates (2.5/s) are exact over time and the same on every
  system. A `duration` (or none: loop for ever) and a `start delay` bound the emission; `looping` restarts it.
- **FR-5 Bursts.** A description may list up to 8 **bursts** — at a time after the start, a count (a min..max range),
  a number of cycles (1..65535 or for ever) and an interval — fired by the emitter's own clock (Unity's emission
  bursts). A description with rate 0, one burst and no loop is a **one-shot** effect that finishes by itself.
- **FR-6 Emitting on demand.** `emit(n)` creates `n` particles now at the emitter's place; `emit_at(point, n)` (and
  with a transform) creates them at another place without moving the emitter (one "sparks" emitter serves every coin;
  Phaser's `explode(count, x, y)`). On-demand emission ignores `emitting` (a stopped emitter still bursts on demand).
- **FR-7 Lifecycle.** `start()`, `stop()` (no more rate or burst emission; live particles finish), `clear()` (every
  particle removed at once), `restart()` (clear, reset the clock **and the random sequence** (FR-15), start: the same
  calls afterwards give the same particles bit for bit), `emitting()`, `alive()` (live count) and `finished()` (not
  emitting by rate or burst any more and no live particle — a one-shot effect is over). **Prewarm**: a description may
  ask for `prewarm` frames simulated at creation and at each `restart()` (Unity's prewarm, Godot's `preprocess`),
  bounded (≤ 600 frames; the cost is documented as `frames × alive`).
- **FR-8 Shapes.** Particles are born on a **shape** in the emitter's local space: a **point**; a **box** (area or
  edge) of a size; a **circle** (area, or ring between an inner and outer radius, optionally an arc); a **line segment**
  of a length. Positions are uniform over the area or along the edge (area-weighted for the circle and ring).
- **FR-9 Direction and speed.** A particle's initial velocity is a speed (range) along a direction: either the
  emitter's **angle ± spread / 2** (a cone; spread 2π is every direction), or **outward from the shape's centre** (an
  explosion from a ring), plus an optional part of the emitter's own velocity (**inherit velocity** 0..1, from its
  movement since the last update). Speeds may be negative.
- **FR-10 Smooth emission from a moving emitter.** Particles emitted by rate in one update are spread along the
  emitter's movement since the previous update (position and rotation interpolated) and pre-aged by their fraction of
  the frame, so a fast emitter leaves an even trail instead of one clump per frame (Godot's `fract_delta`). On-demand
  emission (FR-6) is at the given place, age 0.

### Particles over their life

- **FR-11 Motion.** Each update, in a fixed documented order (semi-implicit Euler per 60 Hz frame): velocity += gravity;
  velocity × (1 − drag) per frame (drag 0..1 from a per-second **damping** value); position += velocity; rotation +=
  angular velocity. Initial rotation and angular velocity are ranges. A particle dies when its age reaches its
  lifetime (a range); its age is in frame-clock ticks, so a lifetime of `Frames{24}` lives exactly 24 updates.
- **FR-12 Appearance over life.** A particle's drawn **colour** (tint and alpha) is a **gradient** of up to 8 keys
  (position 0..1 of its life, RGBA colour), and its **size** is a curve of up to 8 keys (0..1 of life, multiplier),
  both piecewise linear, times a start colour and a start size (ranges). With one key or none, the value is constant.
  The evaluation is within 1/256 of the exact piecewise-linear value (the plan may precompute tables at creation) and
  the same bits on every system.
- **FR-13 Sprite frames.** An emitter draws its particles with a texture the game gives when drawing (FR-17; the
  emitter owns no texture, as sprite libraries) and a list of up to 16 **frames** (regions of that texture and a
  pivot; default: the whole texture, centred). The frame of a particle is: the first; a **random** frame chosen at
  birth; frames played **over its life** (first to last, optionally repeated n times); or frames at a fixed **rate**
  from birth. A spec 016 sprite library's frames are a documented source of regions (the particles module doesn't
  include the animation header). **Without a texture**, a particle is a solid square of its size in pixels — so
  effects work before art exists and tests need no image.
- **FR-14 Local or world space.** An emitter has a **transform** (spec 022's value: position, rotation, scale; a
  negative scale mirrors) the game sets whenever it wants — from a node's world transform after the hierarchy pass, from
  an entity's body, from the player's feet. In **world space** (default) particles are born at the emitter's current
  transform and then move on their own (a trail stays behind). In **local space** particles live in the emitter's
  frame and are drawn through its transform at draw time (a flame stays on a moving, turning, mirrored torch). Shape,
  direction and gravity are given in local space and mapped through the transform (gravity optionally in world space).

### Determinism and updating

- **FR-15 Seeded and independent.** Every random value comes from the emitter's own **seed** (in the description,
  overridable per emitter) through a counter-based generator (the engine's integer hash of the seed, the particle's
  emission number and the property), never `rand` or the C library. One emitter's randomness never depends on another
  emitter, on draw order, on culling or on how many particles were dropped; `restart()` replays it.
- **FR-16 Same bits everywhere.** The simulation uses only IEEE `+ − ×`, divisions by constants fixed at creation, float↔
  int conversions and spec 022's libm-free sine and cosine, under core's `-ffp-contract=off`, in a fixed order (FR-11,
  emission order). The same descriptions, seeds and calls give the same particle positions, velocities, ages, colours,
  sizes, rotations and frames bit for bit on PC (every supported host compiler) and on the 3DS. Rotated texels stay
  each backend's rasterisation (spec 004), as for sprites.
- **FR-17 Update.** `update()` advances an emitter by one frame: emission (FR-4, FR-5, FR-10), motion and life (FR-11),
  removal of dead particles. It never calls game code, allocates or logs in the normal path. **Pausing** an emitter is
  not calling it (or binding it to a scene, below); a paused emitter still draws its particles where they are. An
  emitter may be **bound to a scene** through D30 unchanged (as tween sets): `BeforeUpdate` runs its `update()` (so a
  pause scene pushed on top freezes it) and `AfterExit` clears it; an unbound emitter is updated where the game wants
  (normally after moving things, before drawing; the order documented with spec 022's pass and the camera rig).
- **FR-18 Order and the full case.** Live particles are kept in **emission order**; drawing visits them oldest first
  (newest on top, default) or newest first (a description value). An emission that finds the emitter full creates as
  many as fit and **drops** the rest, counted in its stats; it is a budget, not a bug — no assert, no log per frame (one
  Debug/Development warning per emitter the first time it drops, plan's choice).

### Drawing

- **FR-19 One command per emitter.** Drawing an emitter on a screen (`draw(screen, texture or none, options)`: layer
  0–15, `ignore_camera`, an opacity and tint multiplied into every particle) records **one** draw-list command, sorted
  with sprites and tile batches by layer and call order (D26), so particles sit between map layers. It takes one of
  `max_draws`. At submit, core reads the emitter **then** (its state after the frame's updates), skips it entirely when
  its bounds (kept by `update()`) are outside the screen's view, and expands only particles whose quad touches the view
  into the platform's 2D drawing (D39, Decisions to take); each particle is drawn exactly as a `gfx::draw` of its frame
  with its position, rotation, scale and tint would be (pixel-art rounding, camera, **zoom** per D33). An emitter
  destroyed or moved after its draw call is forgotten safely (as tile batches, D26), and a destroyed texture is skipped
  with an assert (as sprites). An emitter may be drawn on both screens (a tall world) or several times.
- **FR-20 A particle budget per frame.** Particle quads per frame (both screens) are capped by a new
  `AppConfig::gfx.max_particles` (default **0**: no particle drawing and no memory, as `max_tiles`); its memory is stated
  by the plan. Drawing with 0 asserts with the fix in the message. Over the cap, Debug/Development **assert** and
  Release drops the extra particles and logs one warning per frame — the same rule as `max_draws` and `max_tiles`; the
  simulator drops exactly what the 3DS would. `gfx::stats()` gains particles drawn last frame, the cap and particles
  dropped since `init()`; the debug overlay shows the line when the cap is not 0.

### Presets, queries, cost, safety, opt-in

- **FR-21 Presets from data assets.** The description can be loaded from a spec 014 data asset (for example
  `assets/fx/sparks.particles`) with `data::load`, checked by the build with `file:line` errors (ranges, key order,
  counts) like any data file, through an **opt-in schema source** the engine ships and a game lists in its
  `DATA_SCHEMA` (the plan names it and the extension). The particles module itself never includes or links the data
  module; a game that doesn't list the schema source gets no data code. Editing a preset runs no C++ compile.
- **FR-22 Queries and stats.** An emitter answers its description, transform, capacity, live count, `emitting()`,
  `finished()`, its bounds after the last update, and a read-only view of a particle's position, colour, size, rotation
  and frame by index (for tests and the self-check). Stats: live, peak, capacity, emitted, dropped (full), drawn and
  culled last frame — a debug overlay line (the example shows it).
- **FR-23 No events.** The engine publishes no event for births, deaths or a finished effect (constitution IV: state
  is queried; `finished()` answers "is the explosion over"). Particles have no per-particle callbacks.
- **FR-24 Bounded cost, no allocation while playing.** After an emitter's creation nothing allocates; `update()`,
  emission and drawing call no `virtual` function and no game code, and the per-particle loop doesn't branch on
  description options that are fixed for the emitter (the plan chooses how: precomputed flags, specialised loops).
  Memory per particle is stated by the plan (expected ≤ 48 bytes on the 3DS: 1024 particles ≈ 48 KB). Costs measured on
  an Old 3DS (AC-7).
- **FR-25 Misuse is loud in Debug, harmless in Release.** A description out of range at creation is an error
  (`Result`); `set_desc` with one asserts and changes nothing; NaN or infinite transforms, points or values, `emit`
  with a negative count, `emit` or `update` on an empty or moved-from emitter, drawing outside a frame, an unsorted
  gradient — assert in Debug/Development and do nothing (or clamp, documented) in Release; never undefined behaviour.
- **FR-26 Optional and light.** Particles are an optional module (XI) with one public header that includes only
  `<nnh/math.hpp>`, `<nnh/time.hpp>`, the gfx types it needs and spec 022's light transform header; it includes no
  entities, hierarchy, animation, tween, tilemap, collision or data header. `gfx` never includes the particles module (a
  batch is opaque to it, as tile batches). A game that doesn't include the header links nothing new and
  `examples/hello` keeps its size on PC and 3DS; with `max_particles = 0` every existing example draws the same bytes.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: creation errors and capacities; default and
  out-of-range descriptions; rate accumulator (fractional rates over 600 frames, start delay, duration, looping);
  bursts (times, cycles, intervals, count ranges); `emit`/`emit_at` on a stopped emitter; lifecycle (`stop` lets
  particles finish, `clear`, `restart` reproduces the first run bit for bit, `finished` of a one-shot, prewarm equals
  the same frames run by hand); each shape's positions inside the shape (and on the edge / in the ring / on the arc),
  cone and outward directions, inherit velocity; sub-frame spreading along a moving and turning emitter; motion order
  and exact lifetimes in frames; gradient and size curves at keys and between them (1/256 bound); frame modes; local
  vs world space under a moving, rotated and mirrored transform; emission order kept through deaths, oldest/newest-first
  order; full emitter (as many as fit, the rest dropped and counted, no assert); randomness independent of other
  emitters, culling and drops; `set_desc`; stats; scene binding through D30 (updated before update, frozen under a
  pause scene, cleared after exit). Drawing on the fake platform: one command per draw, sorted with sprites and tile
  batches by layer and call order, per-particle items equal to the equivalent `gfx::draw` items (positions, rounding,
  rotation, scale, tint, frame, zoom 0.5/1/2, `ignore_camera`), whole-emitter and per-particle culling, `max_particles`
  0 asserting, over the cap (assert / Release drop and one warning), an emitter destroyed after its draw call, a
  destroyed texture, texture-less squares. Misuse cases (FR-25). The Linux build VM runs them with GCC 13/14 and
  clang 18 and with ASan/UBSan, as previous specs did.
- **AC-2 Golden simulation.** A test runs a fixed script of 3600 frames over 8 emitters (every shape, rate, bursts and
  one-shots, `emit_at` at seeded points, world and local space, a moving, turning and mirrored emitter, prewarm, stops,
  restarts, `set_desc`, one emitter kept full) and compares, every 600 frames, a checksum of every live particle's
  position, velocity, age, colour, size, rotation and frame bits with golden values. The same test passes on every host
  compiler and in the console self-check (AC-6).
- **AC-3 Data presets.** Fixtures in `tests/data/` (converted by `nnh-assetc data`): a valid `.particles` preset loads
  into a description equal, field for field, to the same description written in C++ (and its emitter gives AC-2's
  bits for the same script); invalid presets (unsorted gradient keys, too many keys, out-of-range values, an unknown
  shape) fail the build with `file:line: reason`.
- **AC-4 Example `examples/particles`** (original art or shapes only), modes chosen by `PARTICLES_DEMO`
  (`showcase` (default) | `check` | `bench`): the top screen shows a gallery — a torch (local space, rate, colour
  gradient, frames over life), smoke (rate, drag, growing size), rain (box shape, world space, high rate), a ship moved
  with the Circle Pad leaving an exhaust trail (world space, sub-frame spreading, inherit velocity), an explosion
  (one-shot bursts, outward from a ring) triggered by A, texture-less squares, and one preset loaded from
  `assets/fx/*.particles`. The bottom screen: a **touch** bursts the selected effect at the touched point (`emit_at`),
  buttons cycle the selected effect and toggle world/local space and the draw layer (behind or in front of a tile
  layer); the HUD shows the emitters' stats and `gfx::stats()`'s particle line. PC screenshots for the gallery, a trail,
  an explosion, a touch burst, local vs world space and a full emitter, checked by reading the PNGs.
- **AC-5 The platformer gets particles without changing its replay.** Dust when the player lands and when it starts
  running, a puff where a turret's bullet hits a wall or the player, and a muzzle flash at the turret (spec 022's muzzle
  point when that spec is implemented, the turret's centre before) are particle emitters outside the world, updated and
  drawn by the level scene. They touch no entity, tween or value in the replay's sample, so the replay's checksum lines
  in `examples/platformer/tests/level.nnhs` and the ctest `platformer_replay_matches_script` pass **unchanged** — no
  golden line is regenerated; the coin sparkles stay spec 021's prefabs (see Decisions to take). The level's 60 Hz
  verdict still passes on the console; the benchmark, gate, zoom and flip demos are untouched. A PC screenshot shows
  landing dust and a bullet puff.
- **AC-6 Console script.** The `check` mode runs AC-2's script on the engine's real emitters and logs
  `particles: <frame> sum <checksum>` every 600 frames and `particles: PASS <checksum>` or `particles: FAIL ...` at the
  end, against golden values recorded from the simulator, then draws its last frame and logs `gfx::stats()`'s particle
  line. `examples/particles/tests/check.nnhs` starts it, expects every line, then switches to the showcase, **touches**
  fixed points on the bottom screen and expects the logged bursts (for example `burst: sparks 16 at 120,64 live 16`)
  and, a fixed number of frames later, `live 0` and `finished`, and takes screenshots. On an Old 3DS the maintainer
  only opens the Homebrew Launcher and presses Y, and `nnh run particles --script examples/particles/tests/check.nnhs`
  passes; the same script passes with `--pc`. The verdict, log and screenshots are in the saved log folder.
- **AC-7 Cost on the console.** The `bench` mode, measured on an Old 3DS with the profiler and logged as
  `particles bench: PASS <numbers>` (or FAIL). **Targets** (the plan confirms them before code; they predate any
  console measurement, and are estimated from the Old 3DS's 268 MHz ARM11 (VFPv2, no NEON), its 16 KB data cache with
  32-byte lines, no L2, no hardware prefetcher and ~110 cycles per miss: 1024 particles of ~48 bytes stream ~1500
  lines per update, about 0.6 ms of misses alone):
  - `update()` of **1024 live particles** in one emitter in steady state (rate emission, gravity, drag, angular
    velocity, a 4-key gradient, a 3-key size curve, frames over life): **under 1.2 ms**;
  - the same 1024 particles in **16 emitters of 64**: **under 1.4 ms** (per-emitter overhead);
  - **emitting 512 particles in one frame** (32 `emit_at` bursts of 16, circle shape, random direction, every range
    random): **under 1.0 ms**;
  - **drawing 512 visible particles** (8×8 frames of one texture, rotated and tinted) on the top screen: a per-particle
    CPU cost (record + expand + submit) **at most 0.75×** a `gfx::draw` sprite with the same options measured in the
    same run, and **512 particles simulated and drawn plus 300 sprites at 60 Hz** with no missed frame of 600 (the
    sprites bench measured about 1300 16×16 sprites alone on an Old 3DS XL);
  - an emitter entirely off-screen costs **no more than its update** (culled at submit).
  If a target is missed, the implementation is optimised first; if it is still missed, the budget is set from the first
  console measurement with margin and written in the spec and the docs with the measured numbers (the maintainer's
  policy for benches, as in specs 019 to 022). Spec 012's tile gate, the sprites bench and the platformer's 60 Hz
  replay verdict still pass.
- **AC-8 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS; `examples/sprites`,
  `examples/scenes`, `examples/camera`, `examples/queries`, `examples/entities` and `examples/prefabs` keep their
  screenshots pixel-identical and their scripts and goldens passing (with `max_particles` 0 their draw lists are the
  same bytes); every earlier spec's tests pass unchanged. A game that includes the particle header but not the preset
  schema links no data code (checked on the 3DS build's map file or symbol list). Building a game `.cpp` that includes
  the particle header stays within constitution III's budgets (measured before and after on `examples/particles`); a
  preset edit rebuilds only data (time measured, as spec 014's).
- **AC-9 Docs.** A new `docs/api/` page for particles (emitters and capacities, the description and its units,
  emission modes, shapes, directions, motion, colour/size/frames over life, local and world space, following an entity
  or a node, sub-frame spreading, determinism and seeds, update and scene binding, drawing, layers, culling,
  `max_particles`, the full case, presets, stats, costs, memory, misuse); `docs/api/gfx.md` (capacities, stats, layers),
  `docs/architecture/rendering.md` (particle batches) and `docs/api/data.md` (the preset schema) updated;
  `docs/api/hierarchy`/transform page and `docs/api/animation.md` link the "follow a node" and "frames from a sprite
  library" patterns; a decision row **D39** in `docs/architecture/decisions.md`; CLAUDE.md layout and ROADMAP updated;
  `docs/REFERENCES.md` citing the prior art below.

## Non-goals

- **GPU particles** (Godot's `GPUParticles2D`, PICA200 geometry shaders): the simulation is on the CPU, the same code
  on every system, which is what makes FR-16 possible.
- **3D particles** and stereoscopic depth per particle (M8).
- **Blend modes** (additive, multiply): `gfx` has only alpha blending today; additive particles need a blend mode in
  spec 004's draw options and the platform contract, a gfx change of its own (open question).
- **Collision** of particles with tilemaps, boxes or the hierarchy, and **death zones** (Phaser): particles pass
  through everything. A follow-up could test a `collision::Grid` (spec 012/019) per particle (open question).
- **Sub-emitters** (Unity's sub-emitters, Godot's `sub_emitter`), **trails and ribbons** (line renderers), **attractors,
  turbulence and noise fields**, radial and tangential acceleration, and **align to velocity** (it needs a
  deterministic `atan2`): a game emits from its own code at a point it knows; follow-ups if games need them.
- **Per-particle callbacks or events** (births, deaths): FR-23.
- **Particles as entities**, an engine emitter component, or emitters stored in worlds: an emitter is a heavy object
  with its own block; an entity or node *drives* one through its transform (FR-14), and one emitter serves many places
  (FR-6).
- **Sorting particles across emitters** or by depth: each emitter is one command in the layer order; within it,
  emission order (FR-18).
- **Time scale** (slow motion per emitter) and **fixed lower simulation rates** (Godot's `fixed_fps`): one update is one
  frame; a game slows an effect by updating it less often.
- **Replacing the oldest particle when full** (Unity's ring buffer mode): new particles are dropped (FR-18; open
  question).
- **An editor or live tuning** in the simulator: presets are text files (FR-21); the "Hot reload of assets" and "Live
  values" ROADMAP items will make them live.
- **Changing the platformer's coin sparkles**: they stay spec 021's prefabs (AC-5).

## Constraints

- **I** — portable core code with no platform part and no platform `#ifdef`; the same calls give the same particles
  on PC and 3DS (FR-16); the example and scripts run in the simulator and on the console.
- **II** — one block per emitter sized at creation; the plan states the bytes per particle (≤ 48 expected), per
  emitter and of `max_particles`' scratch; costs measured on an Old 3DS (AC-7), designed for its cache (streaming
  structure-of-arrays or hot/cold split chosen by the plan from the measurement).
- **III** — one light header (math, time, gfx types, the transform value; no standard containers or `<functional>`);
  `hello` unaffected; build times measured (AC-8); presets are data-only rebuilds.
- **IV** — no engine events (FR-23); state is queried (`alive`, `finished`, stats).
- **V** — no exceptions (`Result` at creation), no RTTI, no `virtual`, no allocation after creation, no per-frame heap;
  the update and draw expansion call no game code.
- **VI** — pure simulation logic unit-tested first on the fake platform with a golden checksum (AC-1, AC-2); drawing
  checked through the fake platform's recorded items; visible behaviour by screenshots and on the console (AC-4 to
  AC-6).
- **VII** — description checks, emission, shapes, motion, curves, the random generator, culling and batch expansion in
  small separate files; one public header for the module.
- **VIII** — header comments, a `docs/api/` page, linked pages, `examples/particles` and the platformer updated.
- **IX** — written from the published designs (Godot's, Unity's, LÖVE's and Phaser's documentation, *Game Programming
  Patterns*); no code copied; original art only; credited in `docs/REFERENCES.md`.
- **X** — the only new external input is the preset, read by spec 014's bounded, fuzzed loader and checked at build
  time; misuse is loud in Debug/Development and harmless in Release (FR-25); NaN or infinite values are caught at the
  call that brings them in.
- **XI, D25** — optional: particles without entities, hierarchies, animation, tweens or data assets; presets without
  forcing data assets on code-only users (FR-21, FR-26); plain sprites and pools stay a supported way to make effects.
- **Under pressure:**
  - **D26 / D33 / rendering.md** — a second kind of batch command in the draw list (D39). Expanding into the existing
    2D items keeps the platform contract unchanged; if AC-7's draw target is missed, the fallback is a new contract
    function with compact particle quads (as `draw_tiles`), a contract change every platform implements.
  - **`AppConfig::gfx`** gains `max_particles` (default 0), a new per-frame budget beside `max_draws` and `max_tiles`.
  - **Spec 022 is a prerequisite** for the transform value and the public deterministic sine/cosine. It is approved but
    not implemented; if spec 024's plan comes first, the plan either waits or moves spec 015's sine to the shared place
    spec 022 describes (the same file spec 022 will use), without a second copy.
  - **D29 (data assets)** has only game-written schema sources so far; FR-21 adds an engine-shipped, opt-in schema source
    a game lists in its `DATA_SCHEMA`. The plan must show it needs no change to `nnh-assetc` or to D29's pipeline, or
    amend D29.
  - **"The same bits on PC and 3DS"** now covers a whole simulation with sine, cosine and a hash generator: FR-16
    relies on `-ffp-contract=off` and the fixed update order; AC-2 exists to catch a compiler or flag that breaks it.
    Rotated texels remain backend rasterisation, so scripts compare logged values, not those pixels.
  - **The maintainer's preference for unchanged replay lines** is kept by AC-5 (particles outside the world). Turning the
    coin sparkles into particles would change the replay's live count and handle hash (Decisions to take).
  - **AC-7's** targets predate any console measurement and fall back to the measurement-with-margin policy.

## Prior art

- **Godot** `CPUParticles2D` (and `GPUParticles2D` with `ParticleProcessMaterial`): an emitter node with `amount`
  (fixed capacity), `lifetime`, `one_shot`, `explosiveness`, `preprocess`, `local_coords`, `fract_delta`, `draw_order`
  (index or lifetime), emission shapes (point, sphere, rectangle, ring, points), `direction`/`spread`, `gravity`,
  `damping`, `angular_velocity`, `color_ramp`, `scale_amount_curve`, sprite-sheet animation; no collision on the CPU
  version — [`CPUParticles2D`](https://docs.godotengine.org/en/stable/classes/class_cpuparticles2d.html),
  [Particle systems (2D)](https://docs.godotengine.org/en/stable/tutorials/2d/particle_systems_2d.html). FR-7's prewarm,
  FR-10's sub-frame spreading, FR-14's spaces and FR-18's order follow it.
- **Unity** Particle System: the main module (Max Particles, Simulation Space local/world, Prewarm, Ring Buffer Mode,
  Emitter Velocity), Emission (rate over time, bursts with count, cycles and interval), Shape, Color/Size over Lifetime,
  Texture Sheet Animation —
  [Main module](https://docs.unity.com/en-us/engine/current/manual/visual-effects/particle-systems/particle-system-modules/part-sys-main-module),
  [Particle System modules](https://docs.unity3d.com/Manual/ParticleSystemModules.html),
  [Emission module](https://docs.unity3d.com/Manual/PartSysEmissionModule.html),
  [Shape module](https://docs.unity3d.com/Manual/PartSysShapeModule.html). FR-5's bursts, FR-9's inherit velocity and
  FR-13's frame modes follow it; the ring buffer is a non-goal.
- **LÖVE** `ParticleSystem`: a fixed buffer size, `emit(n)`, emission areas (uniform, normal, ellipse, border
  ellipse, border rectangle), spread, linear damping, up to 8 colours and 8 sizes over a particle's life, relative
  rotation — [`ParticleSystem`](https://love2d.org/wiki/ParticleSystem). FR-12's 8 keys and FR-8's shapes follow it.
- **Phaser 3** `ParticleEmitter`: flow mode (frequency, quantity) and explode mode (`explode(count, x, y)`), emit zones
  (random or edge), death zones, `stop`/`pause`/`resume` —
  [Particles](https://docs.phaser.io/phaser/concepts/gameobjects/particles),
  [`ParticleEmitter`](https://docs.phaser.io/api-documentation/class/gameobjects-particles-particleemitter). FR-6's
  `emit_at` serving many places follows it; death zones are a non-goal.
- Robert Nystrom, *Game Programming Patterns*: [Object Pool](https://gameprogrammingpatterns.com/object-pool.html)
  (particles as the canonical pooled object, fixed capacity, what to do when full) and
  [Data Locality](https://gameprogrammingpatterns.com/data-locality.html) (particles in contiguous arrays, keeping the
  live ones packed) — FR-1, FR-18, FR-24.

## Decisions to take

Every question has a recommended answer; none is decided until the maintainer approves the spec.

- [x] **What is an emitter, and who owns the particles?** Options: (a) **an emitter object the game creates, one block
      with its own capacity, one description; one emitter serves many places through `emit_at`**; (b) a particle
      system object holding several emitters that share one pool; (c) particles as entities of a spec 020 world. —
      **recommended: (a)** (FR-1, FR-6). It is the shape of Godot's and Unity's emitters and LÖVE's systems, keeps one
      command per emitter for drawing and one contiguous array per simulation loop; (b) needs per-particle emitter ids
      and a sort or partition to draw; (c) costs an entity record, a handle and a component lookup per particle.
- [x] **How is an emitter configured?** Options: (a) **one plain description struct of data-compatible members, with
      defaults, replaceable with `set_desc`**; (b) a builder or setters per module (Unity's API); (c) a text format only.
      — **recommended: (a)** (FR-2). The ROADMAP asks for "a C++ struct"; designated initialisers read like a config;
      data compatibility gives presets (FR-21) for free.
- [x] **Emission modes.** — **recommended: rate (fixed-point accumulator), up to 8 bursts with cycles and intervals,
      one-shot as a description, and on-demand `emit`/`emit_at`; start delay, duration, looping** (FR-4 to FR-6).
      Covers smoke, rain, fountains, explosions and per-event sparks.
- [x] **Shapes.** — **recommended: point, box (area/edge), circle (area/ring/arc), line; directions by cone or outward
      from the centre** (FR-8, FR-9). Emission from a sprite's pixels or a list of points (Godot's points modes) is a
      follow-up.
- [x] **Forces.** — **recommended: gravity, per-second damping, angular velocity, inherit velocity; no radial,
      tangential, attractors or noise** (FR-9, FR-11, Non-goals). Enough for the classic 2D effects at a known cost per
      particle.
- [x] **Values over life.** Options: (a) **piecewise-linear keys (≤ 8) for colour/alpha and size, start ranges, the plan
      free to precompute tables**; (b) spec 015's easings between keys (the particles module would depend on the tween
      module, XI); (c) arbitrary game callbacks per particle (cost, determinism). — **recommended: (a)** (FR-12). LÖVE's
      8 colours and sizes cover common effects; more keys give curved shapes.
- [x] **Sprite frames.** — **recommended: up to 16 regions + pivot; first, random, over life (with repeats) or at a
      rate; regions from a spec 016 sprite library by pattern; texture given at draw; texture-less solid squares**
      (FR-13). No dependency on the animation module; squares make effects and tests art-free.
- [x] **World and local space, following things.** Options: (a) **an emitter transform (spec 022's value) set by the
      game; world space by default, local space by a flag; following an entity or node is the documented pattern of
      copying its world transform**; (b) only a position (no rotation or mirroring of local effects); (c) an emitter
      component attached to nodes automatically. — **recommended: (a)** (FR-14). Mirrored and rotated owners work; no
      dependency on entities or the hierarchy (only on the light transform header).
- [x] **Sub-frame spreading.** — **recommended: yes, for rate emission** (FR-10). A fast trail without it shows a clump
      per frame; the cost is an interpolation per emitted particle.
- [x] **Random numbers.** Options: (a) **a counter-based generator per emitter (the engine's integer hash of seed,
      emission number and property), internal to the module**; (b) a new public `nnh::Random` module used by particles;
      (c) `rand()`/`<random>`. — **recommended: (a)** (FR-15). Independence between emitters, `restart()` replays, and
      dropped or culled particles never shift later values; (c) differs per standard library. A public deterministic
      random generator for games is an open question, not this spec's.
- [x] **Determinism scope.** — **recommended: every particle value bit for bit on PC and 3DS, with spec 022's sine and
      `-ffp-contract=off`; rotated texels excepted** (FR-16, AC-2). Same policy as specs 015, 019 and 022.
- [x] **Who calls `update()`?** Options: (a) **the game, once per frame, or a D30 scene binding (`BeforeUpdate`
      updates, `AfterExit` clears) as tween sets**; (b) the engine automatically every frame for every emitter (a
      hidden registry, no pause); (c) only explicit calls. — **recommended: (a)** (FR-17). Pausing is "not updating";
      binding freezes effects under a pause scene; D30 is unchanged. A bound emitter's rate emission follows the
      emitter's transform of the previous frame (documented; unbind for exact order).
- [x] **Order and the full case.** Options for full: (a) **drop new particles and count them**; (b) replace the
      oldest (Unity's ring buffer); (c) assert. Order: **emission order kept, oldest drawn first by default, newest
      first by a flag**. — **recommended: (a) and emission order** (FR-18). A budget is not a bug; dropping keeps live
      effects intact; emission order is free with a stable compaction during the update and matches Godot's `INDEX`.
- [x] **How are particles drawn? (D39)** Options: (a) one `gfx::draw` per particle (a ~136-byte record, a sort entry
      and one of `max_draws` each; the game could do this today); (b) **a particle batch: one draw-list command per
      emitter draw, sorted by layer like tile batches (D26), read at submit, culled per emitter and per particle,
      expanded through a fixed scratch into the platform's existing 2D items, capped by `max_particles`**; (c) (b) with a
      new contract function `draw_particles` taking compact quads (as `draw_tiles`). — **recommended: (b), with (c) as
      the plan's fallback only if AC-7's draw target is missed** (FR-19, FR-20). (b) skips per-particle records, sorting
      and texture lookups with no contract change; (c) may be faster on the 3DS but every platform implements it.
- [x] **Particle budget.** Options: (a) **a new `AppConfig::gfx.max_particles`, default 0 (no memory), with the
      `max_draws`/`max_tiles` rules**; (b) particles count against `max_draws`; (c) only per-emitter capacities. —
      **recommended: (a)** (FR-20). The ROADMAP's "within the draw budget" becomes a visible number; `hello` pays
      nothing; the simulator drops what the 3DS would.
- [x] **Culling.** — **recommended: always simulate (determinism doesn't depend on the camera); cull at submit per
      emitter by its bounds and per particle by its quad** (FR-19). Godot's visibility rectangle, computed instead of
      declared.
- [x] **Presets from data assets.** Options: (a) C++ only; (b) **an engine-shipped, opt-in schema source for the
      description, listed in the game's `DATA_SCHEMA`, loaded with `data::load` (extension named by the plan, e.g.
      `.particles`)**; (c) a particle file format and loader of its own. — **recommended: (b)** (FR-21). Tuning without a
      C++ compile, build-time checks with `file:line`, no new parser (X); the particles module stays free of the data
      module. If the plan finds D29 can't host an engine-shipped schema source cleanly, fall back to (a) plus a
      documented pattern (the game declares its own preset struct) and say so.
- [x] **Events.** — **recommended: none; `finished()` and stats** (FR-23).
- [x] **Blend modes.** — **recommended: not in this spec (alpha only); additive blending is a gfx follow-up** (Non-goals).
- [x] **Collision.** Options: (a) **none in this spec**; (b) an optional `collision::Grid` per emitter: particles die or
      bounce on solid cells (one cell lookup per particle per frame). — **recommended: (a)** (Non-goals). Keeps the
      module free of collision and the per-particle loop fixed; (b) is the natural follow-up (open question).
- [x] **Sub-emitters.** — **recommended: not in this spec**; a game emits from its own code (Non-goals).
- [x] **Example and platformer.** Options for the platformer: (a) **new cosmetic effects outside the world (landing and
      running dust, bullet puffs, muzzle flash); coin sparkles stay prefabs; replay lines unchanged**; (b) replace the
      sparkles with a particle emitter (fewer entities, but the replay's live count and handle hash change and the lines
      are regenerated); (c) the platformer untouched. — **recommended: (a)** (AC-5), with a new `examples/particles`
      owning the self-check and bench (AC-4, AC-6, AC-7). The sparkles are spec 021's prefab demonstration and the
      replay stays a proof that particles don't touch game state.
- [x] **Old 3DS cost targets** — **recommended: AC-7's targets, confirmed by the plan, and set from the first console
      measurement with margin if they are still missed after optimisation** (the maintainer's bench policy).

## Open questions

- [ ] **A public deterministic random generator** (`nnh::Random`, seeded, the same bits everywhere) for games — useful
      beyond particles (the camera shake has its own). Recommended: a small follow-up spec, not this one; particles keep
      their internal counter-based generator either way.
- [ ] **Particle collision with grids** (die or bounce on solid cells, spec 012/019's `Grid`). Recommended: a follow-up
      after the example shows whether games want it; measure one cell lookup per particle first.
- [ ] **Additive blending.** Recommended: a gfx follow-up (draw options and contract), then a particles flag.
- [ ] **Replace-oldest when full** (Unity's ring buffer) for continuous trails. Recommended: not now; revisit if a game
      asks.
- [ ] **Align to velocity** (rotation follows the direction of motion) — needs a deterministic `atan2`. Recommended: a
      follow-up together with the public random generator, if games need it.
- [ ] **Bytes per particle and layout** (≤ 48 bytes proposed; SoA vs hot/cold split) and `max_particles`' memory —
      the plan fixes them from a first Old 3DS measurement.
- [ ] **Particle batches per frame** (tile batches are capped at 64) — the plan fixes the number and whether the two
      kinds share the table.
- [ ] **Preset file extension and schema source name** (`.particles` proposed) — the plan fixes them with D29.
