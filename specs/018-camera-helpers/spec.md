# Spec 018: Camera helpers

Status: implemented, console check (T9.2) and Linux checks (T8.1) pending (2026-11-22). Approved 2026-11-22
(maintainer pre-approved recommendations: every open question below is answered with the recommended answer)
Author: NNHGE maintainers · Date: 2026-11-22
Depends on: [spec 002](../002-core-foundation/spec.md) (asserts, logs, profiling), [spec 004](../004-2d-graphics/spec.md)
(`Camera2D`, `gfx::set_camera`, camera-relative and `ignore_camera` draws; FR-9 extends `Camera2D` with a zoom),
[spec 012](../012-tilemaps-and-collision/spec.md) (tile batches, D26, which FR-9 must scale; map size for limits).
Works with [spec 015](../015-tweens-and-timers/spec.md)'s tweens (a tween can drive the offset or the zoom) and the
frame clock of `<nnh/time.hpp>` (`Frames`, `FrameTime`) without depending on the tween module. Verified on the console
with [spec 013](../013-console-remote/spec.md)'s `nnh run --script`. Implements the ROADMAP item "Camera helpers" of
"Features in the spirit of Godot and Unity". Optional for games (constitution XI, decision D25).

## Problem and goal

Every scrolling game needs a camera that follows something, and today each game writes its own: the platformer's
`follow_camera()` centres the player and clamps to the map, and that is all it does. The camera that makes a game
feel good does more — it lets the player move a little before scrolling (a dead zone), eases instead of snapping
(smoothing), never shows outside the level (limits, also per room), shakes on an explosion and settles back exactly,
zooms in for a cut-in or out for a boss — and on the 3DS it must also decide what the second screen shows: the world
continuing below the top screen (one tall world, as many DS games did) or a view of its own. Each of these is easy to
get subtly wrong (jitter at a limit, a shake that drifts the camera, smoothing that depends on the frame rate, a
touch on the bottom screen mapped to the wrong world point). Godot's `Camera2D` and Unity's Cinemachine solve it with
a configured camera. After this spec a game keeps a small camera value per view, tells it each frame what to follow,
and gets the same positions frame by frame on PC and 3DS (so replays and console checks compare exactly); a game that
sets `Camera2D` by hand keeps doing so and pays nothing.

## User stories

- As a game developer, I want the camera to follow a point or a box (my player) with an optional dead zone and
  smoothing, so that scrolling feels calm without writing the maths myself.
- As a game developer, I want the camera kept inside the level's bounds — taken from my tilemap or given as a
  rectangle, and changeable when the player enters another room — so that the player never sees past the edge.
- As a game developer, I want to add "trauma" on a hit or an explosion and get a shake that fades out by itself and
  never moves the camera's real position, so that juice is one line.
- As a game developer, I want to zoom the world in and out (cut-ins, a boss arena, a map overview) while the HUD stays
  as it is.
- As a game developer on the 3DS, I want to choose whether the two screens show one tall world (the bottom screen
  continues below the top one) or two independent views, so that I can make DS-style vertical games or a top/bottom
  split (game above, map below) with the same helper.
- As a game developer, I want to convert between screen and world points (a touch on the bottom screen to a world
  position, a world position to a screen point for a marker), for any zoom and either screen mode.
- As a game developer, I want to jump the camera instantly (level start, respawn, teleport) without it gliding across
  the level.
- As a game developer, I want to see the dead zone and limits drawn while I tune them.
- As a game developer who prefers my own camera code, I want `Camera2D` to keep working as it does and lose nothing.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### The camera value

- **FR-1 A camera helper is a plain value the game owns.** The engine provides a small camera helper type (the plan
  fixes its name; "the camera" below) that the game keeps as a member of its scene or object, one per view. It holds
  its settings and state inline, has no heap storage, needs no `nnh::init()` for its computations and no set,
  registry or scene binding, and is copyable. The game advances it once per frame with `update()` (FR-12); it then
  answers queries (FR-11) and, when asked, writes its result to a screen's `Camera2D` (FR-13). Settings (FR-2 to FR-10)
  can be changed at any time, including between two updates.

### Following

- **FR-2 Target.** The camera follows a **target** given each update: a point or a box (`Rect`, followed by its
  centre), plus an **offset** in world pixels the game may change at any time (a look-ahead in the facing direction,
  for example tweened with spec 015). Following can be turned off per axis (a horizontal shooter locks y; a platformer
  may follow y only when the player stands on the ground, by passing the last standing height). The desired camera
  centre is the target's centre plus the offset.
- **FR-3 Dead zone.** An optional **dead zone**: a rectangle around the view's centre, given in **screen pixels**
  (so it looks the same at any zoom), inside which the target moves without moving the camera. When the target leaves
  it, the desired position moves just enough to bring the target back to the zone's edge on that axis (Godot's drag
  margins). An empty dead zone (the default) means the target is always centred. Settings that make the dead zone
  larger than the view are clamped to the view and logged once in Debug/Development.
- **FR-4 Smoothing.** Optional per-axis **smoothing**: each update the camera moves a fixed fraction of the remaining
  distance towards the desired position (a frame-based exponential approach; fraction 1, the default, follows
  instantly). When the remaining distance falls below a documented threshold (target: 1/64 pixel) the camera snaps to
  the desired position, so it settles exactly and a still target leaves the camera still, bit for bit. Smoothing is
  per frame (FR-12), not per second, so it cannot depend on the frame rate.
- **FR-5 Instant moves.** `snap` (the plan fixes the name) places the camera at the desired position for a target
  immediately, with no smoothing, keeping limits (FR-6); it is what a game calls at level start, respawn and teleport.
  It does not clear the shake (FR-8); a separate call stops the shake.

### Limits

- **FR-6 Limits.** Optional **limits**: a world rectangle the view must stay inside (Godot's `limit_*`, Cinemachine's
  confiner for rectangles). The desired position is clamped **before** smoothing (so the camera never eases into a wall
  and bounces back) and the result is clamped again after smoothing and shake (FR-8). On an axis where the limits are
  smaller than the view, the camera is placed by a **small-world alignment** setting: start (the view's left/top edge
  at the limit's — today's platformer behaviour), centre (the default) or end. Limits can be changed at any time; a
  game that wants the camera to glide into a new room's limits keeps smoothing on, one that wants a cut calls `snap`.
- **FR-7 Limits from a tilemap.** The tilemap module gains a small query for a map's size in world pixels (its
  bounds starting at (0, 0)), so `camera.set_limits(map.bounds())` (names fixed by the plan) is one line. The camera
  module does not include or link the tilemap module and the tilemap module does not include the camera module
  (constitution XI): the limits are a plain `Rect`.

### Shake

- **FR-8 Trauma shake.** The camera has a **trauma** level in [0, 1] (Eiserloh's model): the game adds trauma
  (`add_trauma(0.4f)`, clamped to 1), it decays linearly by a configurable amount per frame, and the shake offset each
  frame is `max_offset × trauma²` (exponent documented) times a smooth noise value in [-1, 1] per axis, sampled at a
  configurable frequency. Rules:
  - the shake is an offset on top of the followed position: the followed (unshaken) position never moves because of
    it, and once trauma reaches 0 the camera is exactly where it would have been without the shake;
  - the noise is the engine's own (a seeded integer hash and an interpolation with no C math library calls), so the
    same seed, trauma calls and updates give the same offsets, bit for bit, on PC and 3DS; the seed is settable and
    defaults to a fixed value (no global random state, no time-based seed);
  - the shaken position is clamped by the limits (FR-6) unless the game turns that off (then the shake may show up to
    `max_offset` pixels past the limits);
  - translation only: `Camera2D` has no rotation (non-goals).

### Zoom

- **FR-9 Zoom in `Camera2D`.** `Camera2D` gains a **zoom** factor (default 1). With zoom `z`, every camera-relative
  draw on that screen — sprites (position and scale), shapes, world text and tile batches — is drawn `z` times larger
  around the camera's position, so the screen shows `screen size / z` world pixels; draws with `ignore_camera`, debug
  text and the debug overlay are unaffected. Rules:
  - zoom 1 draws exactly as today (same pixels, same recorded data on the fake platform) — no existing game,
    screenshot or golden value changes;
  - the range is [0.5, 4]; values outside are clamped, asserting in Debug/Development;
  - tile layers show **no gaps and no overlaps** between tiles at any zoom in the range (per-tile edges snapped to
    whole screen pixels), and a layer with parallax 1 stays aligned with sprites at the same world position;
  - integer zooms (2, 3, 4) are pixel-exact (each texel becomes an exact square); fractional zooms are allowed and
    documented as uneven pixel sizes, as on any pixel-art engine;
  - visible cells of tile layers are computed from the zoomed view, so zoom 0.5 draws up to four times the tile quads
    of zoom 1, within `AppConfig::gfx.max_tiles` (the simulator drops what the 3DS would, D26).
  The camera helper has a zoom setting that it writes into `Camera2D` (FR-13); its dead zone, limits and screen/world
  conversions use the zoomed view size. Animating the zoom is the game's choice (a spec 015 tween through a setter, or
  setting it each frame); the helper keeps the view centre fixed when the zoom changes (zoom about the centre).

### The two screens

- **FR-10 Screen modes.** A camera works in one of three modes, chosen at any time:
  - **single view**: one screen (top or bottom), the other screen's camera untouched — the default;
  - **one tall world**: one camera drives both screens as if the bottom screen continued below the top one. The
    bottom view sits under the top view, horizontally centred (400 vs 320 pixels wide: inset 40 pixels each side),
    separated by a configurable **gap** in world pixels (the space hidden between the physical screens; default 0).
    Following, dead zone and limits use the combined view (400 wide, 480 + gap tall at zoom 1), both screens share the
    zoom and the shake, and a sprite crossing from one screen to the other moves continuously (it appears on the
    bottom screen exactly where it would be if the screens touched, plus the gap);
  - **two views**: two cameras, each in single-view mode on its own screen (game above, map or second target below).
    Nothing more is needed than two camera values; the mode is named and documented so the choice is explicit.
  On systems with one physical screen the simulator shows both screens as it already does (spec 006), so the modes
  behave the same.

### Update, queries and output

- **FR-11 Queries.** The camera answers: its **followed** position (no shake) and its **final** position (with
  shake), each as the top-left corner the way `Camera2D` uses it; the **visible world rectangle** per screen; current
  trauma; and **conversions** between screen points and world points for a given screen (a touch on the bottom screen
  in tall-world mode becomes the world point under the finger, at any zoom). Conversions match what is drawn: a sprite
  drawn at `to_world(screen, p)` appears under screen point `p` (within the renderer's whole-pixel rounding).
- **FR-12 Deterministic per-frame update.** `update(target)` advances the camera by exactly **one 60 Hz frame**: the
  dead zone, smoothing, trauma decay and noise time move by one step, never by measured wall time. The game calls it
  where it wants, normally in its scene's `update()` **after** moving the target, so the camera shows this frame's
  target. All arithmetic is plain float operations with a documented order (no C math library calls, no
  fused-multiply-add differences between compilers, as for spec 015's easings), so the same settings, calls and
  updates give the same positions, bit for bit, on PC (every supported host compiler) and on the 3DS. Not calling
  `update()` freezes the camera (a pause menu over the level freezes it with no extra code).
- **FR-13 Applying to the screens.** One call writes the camera's final position and zoom into the `Camera2D` of the
  screen(s) its mode drives (both screens in tall-world mode). Applying is separate from updating, so a game can read
  and adjust the result first (or use the computed position with its own drawing). An optional **pixel snap** setting
  rounds the applied position to whole screen pixels (at the current zoom) for pixel art that must not shimmer; it is
  off by default, and the followed/final positions the camera reports are never rounded.
- **FR-14 Debug drawing.** In Debug and Development builds the camera can draw its dead zone, its limits (where
  visible) and its target point on the screen(s) it drives, on the top draw layer, with documented colours; the call
  does nothing in Release (no strings, no code beyond an empty inline function).

### Cost, safety, opt-in

- **FR-15 Bounded cost.** An update costs a fixed, small amount of work (no loops over the world, no allocation, no
  virtual call, no logging in the normal path) and is a profile scope. The camera value is small (target: at most 128
  bytes, the plan states the size). Misuse (zoom out of range, NaN or infinite targets, offsets or limits, a negative
  dead zone, trauma decay ≤ 0) asserts in Debug/Development and is clamped or ignored with an error log in Release,
  never undefined behaviour.
- **FR-16 Optional and independent.** The camera helper is its own optional module. A game that doesn't use it links
  no camera code (checked on executable size) and its frames cost the same; `Camera2D` set by hand keeps working,
  including with zoom. The module depends on the math types and, for FR-13/FR-14 only, on `<nnh/gfx.hpp>`; it needs
  no other optional feature (tilemaps, tweens, actions) and none needs it. The zoom of FR-9 is part of the graphics
  module and costs games that leave it at 1 nothing measurable (the tile gate of spec 012 is unchanged).

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: following a point and a box, offsets, per-axis
  locks; the dead zone (target inside: still; leaving each edge: moved by exactly the overshoot; clamped when larger
  than the view); smoothing (fraction, snap threshold, a still target settles to the exact bits, per-axis); `snap`;
  limits (clamp before and after smoothing, start/centre/end alignment when the world is smaller, limits changed at
  run time with and without smoothing); trauma (add, clamp, linear decay to exactly 0, offset bounded by `max_offset`,
  same seed → same offsets, different seeds → different offsets, final position equals the followed one once trauma
  is 0, shake inside limits and with the clamp turned off); zoom (view size, dead zone in screen pixels, zoom about the
  centre); the three screen modes (bottom view placement with the 40-pixel inset and the gap, combined limits);
  conversions round-tripping for both screens, every mode and zooms 0.5, 1, 2, 3; pixel snap; misuse cases (FR-15).
- **AC-2 Golden values.** A test drives a camera through a fixed script (a target path with jumps and stops, dead
  zone, smoothing, limits changed mid-way, trauma, zoom changes, tall-world mode) for 600 updates and compares a
  checksum of the final positions' bits against a golden value; the same test passes on every host compiler and in
  the console self-check (AC-6).
- **AC-3 Zoom in the renderer.** Fake-platform tests on the recorded and submitted draw data: zoom 1 produces exactly
  the data it produced before this spec (a regression test written before the change); at zooms 2 and 0.5 sprites,
  shapes, text and tile quads have the expected positions and sizes, `ignore_camera` draws and debug text are
  unchanged, tile quads of a layer leave no gaps or overlaps at zooms 0.5, 0.75, 1.5, 2 and 3, and the number of
  visible cells follows the zoomed view.
- **AC-4 Platformer adopts the helper.** `examples/platformer` replaces its hand-written `follow_camera()` (and its
  test) with the camera helper, in two documented steps:
  1. **Identical configuration first** — target the player's box, no dead zone, no smoothing, limits from the map with
     start alignment: the camera's positions are the same bits as before, so the replay's checksum lines in
     `examples/platformer/tests/level.nnhs` and the ctest `platformer_replay_matches_script` pass **unchanged**. This
     proves the swap.
  2. **Then the feel changes** — the playable levels get a dead zone, smoothing on y, a look-ahead offset and a small
     shake when the player lands from a high fall. The replay sample keeps the camera position in its checksum (it
     must: the camera is part of what the console must reproduce), so the expected lines change: they are regenerated
     from the PC with the procedure written at the top of `level.nnhs` (`PLATFORMER_DEMO=replay
     NNH_EXIT_AFTER_FRAMES=920 ...`), the ctest passes again, and the console run of `level.nnhs` must print the new
     PC lines. The commit that changes the feel changes the golden lines in the same commit and says why.
  The benchmark and flip demos keep setting `Camera2D` by hand (constitution XI, shown in the same example).
- **AC-5 Example `examples/camera`** (original art or shapes only): the top screen follows a square the player moves
  (Circle Pad / D-Pad) through a small tilemap, with the dead zone and limits drawn (FR-14); buttons add trauma, cycle
  zoom (0.5, 1, 2, 3), toggle smoothing and switch the screen mode between single view (bottom: HUD with the current
  settings), one tall world (the square crosses from the top screen to the bottom one) and two views (the bottom
  screen follows a second, automatic target); a touch on the bottom screen places a marker at the world point under
  the finger (FR-11). PC screenshots (`NNH_EXIT_AFTER_FRAMES` + `NNH_SCREENSHOT`, deterministic frames) for zoom 1,
  zoom 2, zoom 0.5 and tall-world mode, checked by reading the PNGs (no seams between tiles; the square continuous
  across the screens).
- **AC-6 Determinism on the console.** `examples/camera` has a self-check mode that runs AC-2's script on the engine's
  real camera and logs `camera: <frame> sum <checksum>` every 60 frames and `camera: PASS <checksum>` or
  `camera: FAIL ...` at the end, against golden values recorded from the simulator. `examples/camera/tests/check.nnhs`
  starts it, expects every line, and takes screenshots of the zoom and tall-world states; on an Old 3DS the maintainer
  only opens the Homebrew Launcher and presses Y, and `nnh run camera --script examples/camera/tests/check.nnhs`
  passes; the same script passes with `--pc`. The verdict and screenshots are in the saved log folder.
- **AC-7 Cost on the console.** Measured on an Old 3DS with the profiler: updating and applying two cameras (tall
  world, with smoothing, dead zone and trauma) costs under 0.05 ms per frame (target; the plan confirms it). The tile
  gate of spec 012 (4 layers + 300 sprites) still passes at zoom 1 with the zoom support built in; the gate's limits at
  zooms 0.5 and 2 are measured and written in the docs (no pass threshold: zooming out is the game's cost choice).
- **AC-8 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS (no camera helper
  code linked; any growth from FR-9 in the graphics module is stated by the plan and stays within it); `examples/sprites`,
  `examples/scenes`, `examples/tweens` and the platformer's benchmark keep their screenshots pixel-identical.
- **AC-9 Docs.** `docs/api/camera.md` (following, dead zone, smoothing, limits, trauma shake, zoom, screen modes with
  a picture of the tall-world layout, conversions, pixel snap, debug drawing, determinism, costs), `docs/api/gfx.md`
  and `docs/architecture/rendering.md` updated for `Camera2D` zoom, `docs/api/tilemap.md` for the bounds query, a
  decision row in `docs/architecture/decisions.md` if the plan adds one (zoom in the tile batch contract), CLAUDE.md
  layout and ROADMAP updated, `docs/REFERENCES.md` citing Godot `Camera2D`, Cinemachine and Eiserloh's talk.

## Non-goals

- Camera rotation (and rotational shake): `Camera2D` has none; a follow-up together with rotated tile layers if a game
  needs it.
- 3D cameras (M8) and stereoscopic depth: separate types later (rendering.md).
- Several targets framed at once (Cinemachine's target group, auto-zoom to fit players), split screen beyond the two
  physical screens, dynamic split/merge of views.
- Non-rectangular confiners (polygons), camera paths/rails, cut-scene timelines, look-at blending between several
  virtual cameras (Cinemachine's brain).
- Spring/critically damped smoothing, max-speed caps and per-direction look-ahead built in: the fraction-per-frame
  smoothing plus a game-driven offset covers common cases; more models are follow-ups if games ask.
- Platform-specific vertical behaviour ("only move y when grounded") as a built-in: the game passes the target it
  wants (FR-2).
- Scene binding (D30) or automatic updates: the camera is updated by the game after moving its target (open
  questions).
- Engine events (`CameraReachedLimit`, …): state is queried (constitution IV).
- Render-to-texture, post effects (flash, fade, tint) — scene transitions are a separate ROADMAP item.

## Constraints

- **I** — the same calls give the same positions on PC and 3DS; the camera helper is portable core code; zoom is
  applied by core when it submits draws, with the per-platform part limited to what the tile contract needs (FR-9).
- **II** — no storage beyond the camera value (FR-15); zoom-out multiplies visible tiles and is bounded by
  `max_tiles` and the measured limits (AC-7); the update fits the Old 3DS's frame with room to spare.
- **III** — the public header stays light (math types, no `<cmath>` in the header if avoidable); building
  `examples/hello` is unaffected.
- **IV** — the camera is queried, never published every frame; it publishes no events.
- **V** — no exceptions, no RTTI, no `virtual`, no allocation anywhere (not even at creation).
- **VI** — the helper is pure logic, unit-tested first on the fake platform; zoom in the renderer is tested on the
  fake platform's recorded data first, then checked by screenshots and on the console.
- **VII** — following, limits, shake, screen modes and conversions in small separate files; one public header.
- **VIII** — header comments, `docs/api/camera.md`, `examples/camera`, the platformer updated.
- **IX** — the noise and smoothing are written from the published ideas (Eiserloh's talk, value/Perlin-style noise);
  no code copied from Godot (MIT) beyond ideas, credited in `docs/REFERENCES.md`.
- **X** — misuse is loud in Debug/Development and harmless in Release (FR-15).
- **XI, D25** — entirely optional; `Camera2D` by hand stays fully supported; the camera and tilemap modules do not
  depend on each other (FR-7).
- **Under pressure:** FR-9 changes a foundation type (`Camera2D`) and the tile batch path of D26 ("a tile drawn
  unscaled at a whole-pixel position"): the plan must show that zoom 1 is byte-identical (AC-3), that the platform
  contract change is minimal (a scale or a per-tile size for batches), and re-run spec 012's gate (AC-7); a decision
  row records it. AC-4 step 2 deliberately changes the platformer's golden replay lines; the regeneration procedure
  already exists and the console must reproduce the new lines.

## Prior art

- Godot `Camera2D` ([docs](https://docs.godotengine.org/en/stable/classes/class_camera2d.html)): drag margins (FR-3),
  position smoothing (FR-4), `limit_left/top/right/bottom` and limit smoothing (FR-6), `zoom`, `offset` (FR-2),
  `force_update_scroll`/`reset_smoothing` (FR-5), editor drawing of limits and margins (FR-14).
- Unity Cinemachine ([manual](https://docs.unity3d.com/Packages/com.unity.cinemachine@2.10/manual/index.html)):
  the Framing Transposer's dead zone and soft zone with damping, Confiner 2D (FR-6), Impulse for shakes (FR-8).
- Squirrel Eiserloh, "Math for Game Programmers: Juicing Your Cameras With Math", GDC 2016
  ([slides PDF](http://www.mathforgameprogrammers.com/gdc2016/GDC2016_Eiserloh_Squirrel_JuicingYourCameras.pdf),
  [GDC Vault](https://www.gdcvault.com/play/mediaProxy.php?sid=1023146)): trauma in [0, 1], shake = trauma² or
  trauma³, linear decay, smooth noise instead of random values, shake added to a copy of the camera (FR-8).
- DS/3DS games that treat both screens as one tall playfield with a gap between them (FR-10); the gap is a game
  choice, so it is a setting.

## Open questions

All answered with the recommended answer, pre-approved by the maintainer (2026-11-22).

- [x] **Is zoom in scope, given that `Camera2D` and tile batches have no scale today?** Options: (a) in this spec, as a
      `Camera2D` field applied by gfx at submit (renderer and tile contract change); (b) a follow-up spec; (c) zoom
      only inside the helper by scaling positions (sprites would not scale: wrong). — **answer: (a)** (FR-9). The
      ROADMAP item names zoom, a zoom that skips tile layers or sprites is useless, and doing it in `Camera2D` lets
      hand-written cameras zoom too (XI). The cost is contained by AC-3's byte-identical zoom-1 test and AC-7's gate
      re-run; the plan may stage it as the last phase so following/limits/shake land first.
- [x] **Zoom range and pixel exactness** — **answer: [0.5, 4], any value, integer zooms pixel-exact, no gaps between
      tiles at any zoom.** Below 0.5 the tile cost on an Old 3DS explodes (×4 quads at 0.5 already); above 4 a 400×240
      screen shows less than 100×60 world pixels. Restricting to integers would forbid smooth zoom tweens.
- [x] **Who updates the camera, and when? Scene-bound like tweens (D30) or by the game?** — **answer: the game calls
      `update(target)` in its scene's `update()` after moving the target** (FR-12). D30's `BeforeUpdate` runs before
      the scene moves its player, so a bound camera would show the previous frame's target (a one-frame lag that is
      visible at speed). An explicit call also makes the order obvious and keeps the camera a plain value with no
      attachment. A pause menu freezes it for free (the level's `update()` doesn't run).
- [x] **Smoothing model** — **answer: a fixed fraction of the remaining distance per frame, per axis, with a snap
      threshold** (FR-4). It is what Godot's position smoothing does at a fixed rate, it is two float operations per
      axis (bit-identical without libm), and it settles exactly. Springs, max speed and soft zones are follow-ups.
- [x] **Dead zone units: screen or world pixels?** — **answer: screen pixels** (FR-3), so the feel doesn't change
      when the game zooms; the helper converts with the zoom.
- [x] **Limits vs smoothing vs shake: order of clamps** — **answer: clamp the desired position before smoothing, then
      the final (smoothed + shake) position again; shake clamped by default with an option to let it pass the limits**
      (FR-6, FR-8). Clamping before smoothing avoids easing into a wall and bouncing; clamping after keeps the default
      "never show outside the level". At an edge the shake becomes one-sided, which is the usual compromise; games
      with a border of decoration can turn the clamp off.
- [x] **World smaller than the view** — **answer: an alignment setting, start/centre/end, default centre** (FR-6).
      Centre is the usual expectation (Godot users often ask for it); start reproduces today's platformer exactly
      (AC-4 step 1).
- [x] **Shake model and randomness** — **answer: Eiserloh's trauma (trauma², linear decay per frame) with the engine's
      own seeded smooth noise; no global random number generator** (FR-8). Trauma accumulates naturally from several
      hits; seeded noise keeps replays exact. A "shake for N frames with amplitude A" helper is one line of game code
      on top (add trauma, set decay) and is shown in the docs rather than added as API.
- [x] **Tall-world layout: inset and gap** — **answer: bottom view centred under the top (40-pixel inset each side),
      gap a setting in world pixels, default 0** (FR-10). Centring matches the consoles' physical layout; the right
      gap depends on the game's art (some DS games hide ~ a screen's border worth, others none), so it isn't fixed.
- [x] **Map bounds for limits: who provides them?** — **answer: a small `Tilemap` query returning its world-pixel
      bounds; the camera takes a plain `Rect`** (FR-7). Neither module depends on the other (XI); today games compute
      `width() × tile_width()` by hand, which the platformer does twice.
- [x] **Pixel snapping of the applied camera** — **answer: off by default, a setting** (FR-13). The renderer already
      rounds each sprite's final position and snaps tile layer origins (rendering.md); snapping the camera too changes
      the platformer's values (AC-4 step 1 must be byte-identical), and smooth sub-pixel scrolling is preferable for
      non-pixel-art games. Pixel-art games that see shimmer turn it on.
- [x] **Platformer replay checksum when the camera's output changes** — **answer: keep the camera in the checksum;
      adopt in two steps** (AC-4). Step 1 proves the swap with unchanged golden lines; step 2 changes the feel and
      regenerates the golden lines from the PC in the same commit, with the console required to match them. Dropping
      the camera from the checksum would hide exactly the kind of divergence the replay exists to catch.
- [x] **Example: new `examples/camera` or only the platformer?** — **answer: both** (AC-4, AC-5). The platformer shows
      the helper in a real game and keeps its benchmark on a hand-set `Camera2D` (XI); `examples/camera` shows every
      setting, the screen modes and the zoom, and owns the console self-check.
- [x] **Module and header** — **answer: its own optional module `<nnh/camera.hpp>`** depending on `<nnh/math.hpp>`
      and `<nnh/gfx.hpp>` (for `Camera2D`, `Screen` and FR-13/FR-14); zoom lives in the graphics module (FR-9). The plan
      checks the header cost.
