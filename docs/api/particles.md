# Particles

`<nnh/particles.hpp>` (spec 024, decision [D39](../architecture/decisions.md)): sparks, dust, smoke, rain, trails and
explosions from **emitters** the game creates and owns. An emitter is one block of memory with a fixed capacity,
described by one plain struct, updated once per 60 Hz frame and drawn with one draw-list command per draw. The
simulation gives the same bits on PC and 3DS (seeded, libm-free), allocates nothing after creation and calls no game
code. Optional: a game that doesn't include the header links nothing of it.

Demo: [`examples/particles`](../../examples/particles/main.cpp) (gallery, touch bursts, a preset, the self-check and the
bench); the platformer's landing dust, bullet puffs and muzzle flashes
([`level_effects.cpp`](../../examples/platformer/level_effects.cpp)).

```cpp
#include <nnh/particles.hpp>

nnh::ParticleDesc sparks;
sparks.shape = nnh::ParticleShape::Circle;
sparks.radius = 4;
sparks.spread = nnh::kFullTurn;             // every direction
sparks.speed = {60, 120};                   // px/s
sparks.gravity = {0, 200};                  // px/s²
sparks.lifetime = {nnh::Frames{20}, nnh::Frames{30}};
sparks.color_keys[0] = {0, nnh::colors::yellow};
sparks.color_keys[1] = {1, nnh::rgba(255, 60, 0, 0)}; // fade to transparent red
sparks.size = {2, 3};                       // squares of 2–3 px without a texture

auto fx = nnh::ParticleEmitter::create(sparks, 256); // on_enter: one block, 48 B per particle
// when a coin is taken:
fx.value().emit_at(coin_centre, 16);
// every frame:
fx.value().update();
fx.value().draw(nnh::Screen::Top, {.layer = 3});     // needs AppConfig::gfx.max_particles > 0
```

## Emitters and capacities

`ParticleEmitter::create(desc, capacity, tag)` returns a `Result`: `InvalidArgument` for a description out of range
(the message names the first bad field, e.g. `color_keys[2].at: keys must be ascending`) or a capacity outside
1..8192, `OutOfMemory`. It allocates **one block** of `tag` (default `particles`): about 2.3 KB plus **48 bytes per
particle** (a 32-byte record `update()` touches and a 16-byte one written at birth). An emitter is movable, not
copyable; the default-constructed one is empty (commands assert, queries answer zero). `check(desc)` runs the same
checks without creating. One emitter serves any number of places: `emit_at` at every coin.

## The description

`ParticleDesc` is a plain, trivially copyable struct with designated-initialiser-friendly defaults: the default is a
valid emitter of white 1-pixel particles at a point, 60 frames of life, no motion, no emission until asked. Units:
pixels, pixels per second (velocities) and per second² (gravity), radians clockwise on the y-down screen (as
`DrawOptions::rotation`), `FrameTime` for times (`Frames{24}` or a `Duration`). A range (`ParticleRange`,
`ParticleTimeRange`) is a min and a max; each particle draws uniformly between them.

| Group | Fields |
|-------|--------|
| Emission | `rate` (per second, 0..491 520), `delay`, `duration` (0: no end), `looping`, `bursts[8]` |
| Shape | `shape` (`Point`, `Box`, `Circle`, `Line`), `box`, `edge`, `radius`, `inner_radius`, `arc_start`, `arc`, `length` |
| Direction | `direction` (`Cone`, `Outward`), `angle`, `spread`, `speed`, `inherit_velocity` (0..1) |
| Motion | `gravity`, `world_gravity`, `damping` (0..60 per second), `rotation`, `spin`, `lifetime` |
| Appearance | `color_min`/`color_max`, `size`, `color_keys[8]`, `size_keys[8]`, `frames[16]`, `frame_mode`, `frame_repeats`, `frame_rate` |
| Space, order | `space` (`World`, `Local`), `order` (`OldestFirst`, `NewestFirst`), `seed`, `prewarm` (≤ 600 frames) |

`set_desc(desc)` replaces the description (tables rebuilt: a load-time cost); live particles keep their drawn values, a
bad description asserts and changes nothing. Switching `space` of a live emitter changes what its stored positions
mean: `restart()` after it.

## Emission

- **Rate**: `rate` particles per second from an exact accumulator (2.5/s makes 25 particles in 600 frames on every
  machine). It runs after `delay` and, with a `duration`, until its end; `looping` starts the duration again (the delay
  is waited once).
- **Bursts**: up to 8, each `{at, count{min, max}, cycles (0 = for ever), interval}`; a burst with `count.max == 0` is
  unused. A burst fires in the first frame whose end passes its time, at most once per frame; counts are drawn from the
  seed, the burst and the cycle.
- **One-shots**: rate 0, one burst, no loop: `finished()` turns true when its particles are gone.
- **On demand**: `emit(n)`, `emit_at(point, n)` (the point in the emitter's space, its axes kept) and
  `emit_at(transform, n)` (another place and orientation) create particles now at age 0, even when the emitter is
  stopped. They return how many were made.
- **Lifecycle**: `start()` (an ended or stopped emission starts again: clock, delay, bursts), `stop()` (live particles
  finish), `clear()`, `restart()` (clear + clock, bursts and the random sequence from the start + prewarm: the same calls
  give the same particles again), `emitting()`, `finished()`, `alive()`. `prewarm` frames run at creation and at each
  restart (cost: frames × alive).

## Shapes and directions

Particles are born in the emitter's local space: a point; a box (`box` size, centred; `edge` for the outline, uniform by
length); a circle (`radius`, a ring from `inner_radius`, an arc from `arc_start` over `arc`, uniform by area); a line
(`length` along local x). The direction is the cone `angle ± spread/2` (`kFullTurn`: every way) or, with `Outward`,
away from the centre (a point or the exact centre falls back to the cone); speeds may be negative. In world space a
particle also gets `inherit_velocity` × the emitter's movement over the last update. No square root and no libm: the
same positions everywhere.

## Motion, colour, size and frames over life

Each `update()` moves every particle in a fixed order (semi-implicit Euler per 60 Hz frame): `velocity += gravity`,
`velocity ×= 1 − damping/60` (velocities below 2⁻⁶⁰ become 0, so no value is subnormal), `position += velocity`,
`rotation += spin`; a particle dies when its age reaches its lifetime (`Frames{24}` lives exactly 24 updates).

Colour is the start colour (a random point between `color_min` and `color_max`) times a **gradient** of up to 8 keys
(`at` 0..1 of the life, used keys first and ascending; equal positions make a step), size the start size times a curve
of up to 8 keys; both piecewise linear, exact at the keys and within 1/256 between them; no key means white (or 1),
one key a constant. They are computed from the age when the particle is drawn or read, never stored.

Frames: up to 16 regions of the texture given to `draw()` (`region.w < 0`: unused; an empty region: the whole texture;
`center_pivot` or your `pivot`). `frame_mode`: `First`, `Random` (chosen at birth), `OverLife` (first to last over the
life, `frame_repeats` times) or `Rate` (`frame_rate` frames per second from birth). Frames from a
[sprite library](animation.md) are regions you copy into `frames[]` (the particles module doesn't include the
animation header). Without a texture, each particle is a solid square of `size` pixels.

## World and local space, following things

An emitter has a transform (spec 022's [`WorldTransform2D`](transform.md)): `set_transform(Transform2D)`,
`set_transform(WorldTransform2D)` (a node's world after the hierarchy pass), `set_position(point)`. In **world** space
(default) particles are born at the transform and move on their own: a trail stays behind. In **local** space they
live in the emitter's frame and are drawn through its current transform (a flame on a moving, turning, mirrored torch,
by `gfx::transformed`'s rule). Shapes, directions and gravity are given in local space (`world_gravity`: gravity in
world space). To follow an entity or a [node](hierarchy.md), copy its world transform after moving it (after the
hierarchy's pass), then `update()`.

Rate births in world space are spread along the emitter's movement and turn since the previous update and pre-aged by
their part of the frame, so a fast emitter leaves an even trail. `set_transform(t, true)` / `set_position(p, true)`
(**jump**) cancels that and the inherited velocity across a teleport.

## Determinism and seeds

Every random value comes from the emitter's `seed`, the particle's emission number and a property index through the
engine's integer hash: no `rand`, no libm, no shared generator. Another emitter, the capacity (a dropped particle takes
its number), culling and the draw order never change an emitter's particles; `restart()` replays them; `set_seed()`
changes later ones. The same descriptions, seeds and calls give the same positions, velocities, ages, colours, sizes,
rotations and frames bit for bit on PC (every host compiler) and the 3DS; rotated texels stay each backend's
rasterisation. The example's self-check (`PARTICLES_DEMO=check`, golden `C5EF1A84`) proves it on every system.

## Update and scene binding

Call `update()` once per frame, after moving the things emitters follow and before drawing (after spec 022's pass and
the camera rig). Not calling it pauses the effect (it still draws). `bind(scene)` lets the scene do it (D30): the
scene's `BeforeUpdate` runs `update()` (a pause scene pushed on top freezes the effect) and `AfterExit` clears it;
`unbind()` stops that. A bound emitter's rate births follow the transform you set the previous frame (unbind for the
exact order). Keep a bound emitter alive while bound (a scene member).

## Drawing

`draw(screen[, texture], options)` records **one** draw-list command (`layer` 0..15, `ignore_camera`, `tint` and
`opacity` multiplied into every particle), sorted with sprites and tile layers by layer and call order. The emitter is
read at the end of the frame (its state after the frame's updates); an emitter destroyed after its draw call draws
nothing, a moved one still draws. Each particle is drawn exactly as a `gfx::draw` of its frame (or a `gfx::fill_rect`
of its square) would be: camera, [zoom](gfx.md#zoom) and pixel rounding are the sprites' rule. Squares don't rotate.
Culling is conservative and per frame: a whole emitter is skipped when its bounds, grown by its largest quad, miss the
view; otherwise particles whose quad can't touch the view are skipped (counted in `stats().culled`). The simulation
never depends on the camera. Needs `AppConfig::gfx.max_particles > 0` ([gfx.md](gfx.md#particle-batches)); a frame
region outside the texture asserts and that draw is skipped. An emitter may be drawn on both screens or several times.

## The full case

An emission that finds the emitter full makes what fits and drops the rest, counted in `stats().dropped` — a budget,
not a bug: no assert (one warning per emitter in Debug/Development). Over `max_particles` in a frame is the draw
budget's rule (assert; Release drops and warns once per frame).

## Presets in text files

The engine doesn't ship a preset schema (stage 5 of [plan 024](../../specs/024-particles/plan.md): `FrameTime` isn't a
[data](data.md) aggregate). The pattern: declare your own small preset struct in your data schema source (times in
frames, the values you tune), load it with `data::load` and turn it into a `ParticleDesc`; `create()` checks the result
(unsorted keys and ranges included, named). Editing the file rebuilds data only. See
[`presets.hpp`](../../examples/particles/presets.hpp), [`data_types.cpp`](../../examples/particles/data_types.cpp)
and [`embers.particles`](../../examples/particles/assets/fx/embers.particles).

## Queries and stats

`desc()`, `transform()`, `capacity()`, `alive()`, `emitting()`, `finished()`, `bounds()` (the live positions after the
last update or emission, in world space), `particle(i)` (0 = the oldest: position, velocity in px per frame, rotation,
size, colour, frame, age, lifetime), `stats()` (`live`, `peak`, `capacity`, `emitted`, `dropped`, and `drawn`/`culled` by
the last frame that drew it). No events (FR-23): ask `finished()` and publish your own `ExplosionOver` if you need one.

## Costs and memory

| Item | Cost |
|------|------|
| Per particle | 48 bytes (32 hot + 16 cold) |
| Per emitter | ≈ 2.3 KB + 48 × capacity, one block |
| `max_particles > 0` | ≈ 8 KB of gfx storage; 3DS: ≈ 192 B of `Linear` per quad |
| `update()` | one cache line per particle; Old 3DS estimate ≈ 0.9 ms for 1024 particles (target 1.2 ms) |
| Births | ≈ 700 instructions each with a circle and a full-turn cone (two sines); emitting 512 in a frame is over its 1 ms target on an Old 3DS by the estimate |
| Drawing | one command per draw; the expansion ≈ 1.7 µs per visible particle on an Old 3DS (estimate) plus the platform's per-quad cost |
| Off-screen emitter | its update, plus one bounds test at submit |

Old 3DS numbers are estimates until the console run ([plan 024](../../specs/024-particles/plan.md), stage 10): the
bench is `PARTICLES_DEMO=bench` or Y in the example.

## Misuse

Debug/Development assert and Release does nothing harmful: a bad description to `set_desc`, non-finite transforms or
points, a negative count, any command on an empty or moved-from emitter, `particle(i)` out of range (a zero view),
`bind` twice, drawing outside a frame or with `max_particles == 0`, more than 32 particle draws in a frame. Never
undefined behaviour (fuzzed).

## Pitfalls

- Squares (no texture) don't rotate; textured particles do.
- Switching `space` of a live emitter needs a `restart()`.
- A bound emitter's rate births follow the previous frame's transform.
- `stats().drawn`/`culled` are the last drawn frame's, not the current one's.
