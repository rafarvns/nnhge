# Plan 024: Particles

Status: approved 2026-11-24 (overnight pre-approval)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D39 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 022 (`Transform2D`, `WorldTransform2D`, `to_world`, `transform_point`, `transform_direction`, `inverse`,
`bounds`, `is_finite`, the inline `sin_cos_inline`/`is_finite_bits` of `src/transform/sin_cos.hpp`, the mirror rule of
`gfx::transformed`; D37), spec 004/012/018 (the draw list, tile batches and their split points, zoom; D26, D33), spec
015 (`FrameTime`, `Frames`, D30 scene attachments), spec 014 (data assets; D29, amended here), spec 018 (the camera
shake's integer hash, moved to `src/core/`), spec 002 (asserts, logs, `Result`, tagged memory, profiler), spec 013
(scripts). Built in a worktree from `main` **after spec 022 is merged complete**; specs 023 and 025 are built in
parallel worktrees (see "Merge notes").

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ⚠️→✅ | Pressure point: "the same particles bit for bit" now covers a whole simulation with random numbers, sine and cosine. Answered: a counter-based generator (spec 018's integer finalizer over seed, emission number and property; integers only), spec 022's sine inline, every float operation in core sources compiled with `-ffp-contract=off` in a fixed order, signs and finiteness as integer tests, a velocity flush that keeps values out of the subnormal range (the 3DS flushes to zero, the PC doesn't), and AC-2's golden on every host compiler and the console. One division per particle at birth (`1/lifetime`), IEEE correctly rounded like `×` (flagged, decision 9). Rotated texels stay each backend's rasterisation (spec 004) |
| II 3DS constraint | ⚠️→✅ | Pressure points: AC-7's targets predate any measurement (specs 019–023 showed estimates 1.3–2× low), and the draw path must beat a `gfx::draw` sprite. Answered by the ARM11 cost plan below: a 32-byte hot record per particle (one cache line) and a 16-byte cold one, walked sequentially with `pld`; appearance (gradient, size, frame) evaluated only when drawn or queried; update and expansion loops specialised per emitter (no option branch per particle); no `vmrs`, `__aeabi_*` or `memcpy` in hot loops; expansion into a 7 KB scratch that stays in L1 (the sprite path's cost is mostly misses through 136 B per draw); a disassembly check before the console (T7.6). One target is likely over (emitting 512, risk 1), one tight (the draw ratio). Memory: **48 B per particle** (the spec's expectation), ≈ 2.1 KB per emitter, ≈ 8.5 KB of gfx storage only when `max_particles > 0`, and on the 3DS one citro2d quad (192 B, Linear) per `max_particles` |
| III Build speed | ✅ | One header (`<cstdint>`, `<nnh/math.hpp>`, `<nnh/time.hpp>`, `<nnh/gfx.hpp>`, `<nnh/transform.hpp>`, `<nnh/result.hpp>`), no templates, no standard containers; `-fsyntax-only` within 0.05 s of those headers together (T0.1, T1.6); presets are data-only rebuilds (AC-8, measured) |
| IV Event-driven | ✅ | No events (FR-23): `alive()`, `finished()`, stats |
| V No hidden costs | ✅ | No exceptions (`Result` at creation), no RTTI, no `virtual`, no allocation after creation (the gfx storage at `init()`), no game code called by update, emission or expansion; no log in the normal path (one Debug/Development warning per emitter the first time it drops) |
| VI Spec + test first | ✅ | Pure simulation tested first on the fake platform against hand-computed cases and statistical bounds; drawing through the fake's recorded items and the expansion compared with `DrawList::build()` items for the equivalent `gfx::draw`/`fill_rect`; AC-2's golden; fuzz; mutation pass |
| VII Small files | ✅ | One module (`particles`, 14 sources ≤ 250 lines), one public header; gfx gains one source pair and one inline header; every file one concept |
| VIII Documented API | ✅ | `docs/api/particles.md`; gfx, rendering, data, transform, hierarchy, animation pages linked; `examples/particles`; the platformer; D39 |
| IX Clean and legal | ✅ | Own code from published designs (Godot, Unity, LÖVE, Phaser docs, *Game Programming Patterns*); original art; cited in `docs/REFERENCES.md` |
| X Safe by default | ⚠️→✅ | Pressure point: presets are a new external input and "unsorted keys" must fail the build. Answered: presets go through spec 014's bounded, fuzzed loader; D29 gains two generic opt-in field rules (`.ascending`, `.up_to`) checked by the converter and core's shared validator; `create`/`set_desc` re-check every description (a preset loaded from a stale or hostile file can't reach the simulation unchecked); misuse asserts in Debug/Development and is harmless in Release |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure point: drawing needs the gfx submit loop, which every game links. Answered by D39: gfx reaches the particle batch code only through an ops pointer that the particle batch object sets when a game links it (the "Optional frame hook" pattern); `max_particles` defaults to 0 (no memory); `hello` pays five null checks (budget ≤ 128 B of `.text` on 3DS, ≤ 256 B on PC, flagged, decision 30); the preset schema is a source a game lists in `DATA_SCHEMA`; the particles header includes no entities, hierarchy, animation, tween, tilemap, collision or data header |

## Design

### Overview

```
<nnh/particles.hpp>                                  gfx (never includes the particles module)
  ParticleDesc (plain, data-loadable) ─ create ─┐      DrawList: a particle batch command = tile_batch index 64 + i
  ParticleEmitter (one block)                   │      gfx/particle_batches: ≤ 32 batches per frame, 128-item scratch,
    hot[cap]  32 B: position, velocity,         │        the max_particles budget, the submit loop (draw_2d per chunk),
              rotation, spin, age, lifetime     │        forget(source), stats; reached by gfx_frame.cpp only through
    cold[cap] 16 B: 1/life, size, colour, frame │        g_gfx_particle_ops (set by particle_batches.o when linked)
    desc + derived tables + clock + transforms  │      gfx/sprite_place.hpp: snap + sprite/rect placement, shared by
  update(): clock → motion+compaction+bounds →  │        DrawList::build and the particle expansion (one rule)
            rate births (sub-frame) → bursts    │
  emit / emit_at · start/stop/clear/restart     │      src/core/mix32.hpp: the integer finalizer (from the camera
  draw(screen[, texture], options) ─ record ────┘        shake), shared by the particle generator
       └─ at submit: particles/particle_expand.cpp writes DrawItem2D (Sprite or FillRect), culled, within budget

engine/core/schemas/particles_schema.cpp  (opt-in: a game lists ${NNH_PARTICLES_SCHEMA} in DATA_SCHEMA)
  NNH_DATA_TYPE(ParticleDesc, "particles", ...)  → assets/fx/*.particles, data::load("fx/sparks", desc)
```

### Decision D39: emitters own their particles in one block and simulate them deterministically; drawing is a particle batch expanded at submit into the existing 2D items

**Emitters.** A `ParticleEmitter` is a handle (one pointer) to one tagged block the game owns: a header (state,
clock, counters, the current and previous transform, the scene attachment node, stats), the description, derived
tables, and two arrays of `capacity` records in emission order — **hot** (32 B, 32-byte aligned: position, velocity,
rotation, spin, age and lifetime in frame-clock ticks) and **cold** (16 B: `1/lifetime`, start size, start colour
RGBA8, frame byte, two spare bytes). `update()` reads and writes only hot records, plus cold ones when it moves
survivors; births write both; appearance (colour and size over life, the frame) is a function of age evaluated when a
particle is drawn or queried, never stored. Velocities, gravity and spin are kept per 60 Hz frame internally (the
description's per-second values × 1/60 and 1/3600 at creation), so motion is adds and multiplies only.

**Determinism.** Every random value is `u = (mix(base + k·0x9E3779B9) >> 8) × 2⁻²⁴` with `base = mix(seed ^
mix(n))`, `n` the emitter's emission number (incremented for every particle asked for, dropped ones included) and `k`
a fixed property index; burst counts hash (seed, burst, cycle). `mix` is spec 018's lowbias32 finalizer, moved to
`src/core/mix32.hpp` (camera bits unchanged). Ranges are `min + (max − min) × u`. Angles go through
`sin_cos_inline` (spec 022). Normalising the outward direction of a box or line point uses a fixed reciprocal square
root (an integer seed and three Newton steps: `×` and `−` only, no `sqrt`). Area-uniform discs and rings use no square
root: a disc radius is `R · max(u₁, u₂)`, a ring's is a fixed mixture of a uniform and that maximum (exact density,
three values). The rate is a 16.16-per-second accumulator against `60 × 65536`, exact for every rate it can hold.
Motion order per update is fixed (below). A velocity component whose exponent is below 2⁻⁶⁰ becomes +0 (an integer
test) so no value reaches the subnormal range, where the 3DS (flush-to-zero) and the PC would differ.

**Drawing (D26 generalised).** `draw()` records one `TileBatchRecord`-like `ParticleBatchRecord` (`{expand function,
source = the emitter's block, texture handle or none, tint × opacity, layer, ignore_camera}`) into a gfx-owned table
of 32 per frame, and one draw-list command whose `tile_batch` field holds `64 + index` (tile batches keep 0..63). The
command is sorted like every command; `DrawList::build()` is unchanged — it already turns any `tile_batch ≥ 0` into a
split point. At submit, `gfx_frame.cpp` sends each split below 64 to `submit_tiles()` as today and each one from 64 to
`g_gfx_particle_ops->submit()`, which resolves the texture (stale: skipped with an assert, as sprites), computes the
view, then calls the batch's expand function in chunks of up to 128 `DrawItem2D` (the scratch) within the frame's
remaining `max_particles`, sending each chunk with `platform::draw_2d` between the pass's other calls. The expansion
(`particles/particle_expand.cpp`) reads the emitter **then**, skips it entirely when its bounds, grown by its largest
quad, miss the view, and writes for each particle whose quad box touches the view exactly the item a `gfx::draw` of
its frame (or, without a texture, a `gfx::fill_rect` of its square) would give: the placement code is shared
(`gfx/sprite_place.hpp`, used by `DrawList::build` too), so camera, zoom (D33) and pixel rounding are one rule. A
destroyed emitter calls `gfx_forget_particles(block)` (its batches draw nothing); a moved one keeps its block, so it
still draws. No platform contract change: the 3DS only sizes `C2D_Init` for `max_particles` more objects.

**Hooks, not links.** `gfx_frame.cpp` sees one pointer, `detail::g_gfx_particle_ops` (`{init, shutdown, submit,
end_frame, stats}`), null unless `gfx/particle_batches.o` is linked — which a game does exactly when it calls
`ParticleEmitter::draw` or destroys an emitter; a namespace-scope registrar there sets it before `main()` (D29's static
nodes are the precedent). `gfx_init` lets it allocate when `max_particles > 0`, `gfx_submit` lets it count drops and
warn, `gfx::stats()` lets it fill three fields, `free_all` lets it free.

**Rejected:** one `gfx::draw` per particle (a 136 B record, a sort entry and one of `max_draws` each: the cost D26
measured); a new contract function `draw_particles` with compact quads (every platform implements it; kept as the
fallback if AC-7's draw target is missed, risk 1); storing appearance per particle (8 more bytes written per particle
per frame for values most culled or paused emitters never show); a structure of arrays per field (8 streams per
update, 8 moves per compaction step); a ring buffer (emission order and stable compaction are simpler with random
lifetimes; a follow-up for fixed lifetimes, risk 7); `rand`/`<random>`/a shared generator (per-library bits, emitters
depending on each other); `std::sin`, `std::sqrt`, `std::pow` (per-libm bits; damping is linear per frame instead of
`(1 − d)^(1/60)`); 256-entry value tables for curves (steep gradients miss FR-12's 1/256); a particle system holding
several emitters (per-particle emitter ids, a sort to draw); particles as entities; a hidden registry updating every
emitter; events.

### Public API (sketch)

```cpp
// <nnh/particles.hpp>: <cstdint>, <nnh/math.hpp>, <nnh/time.hpp>, <nnh/gfx.hpp>, <nnh/transform.hpp>, <nnh/result.hpp>.
// Declarations only (every float operation is in src/particles/*.cpp, compiled with -ffp-contract=off). Optional (XI).
namespace nnh {
struct MemTag;   // <nnh/memory.hpp>
class Scene;     // <nnh/scene.hpp>
class Texture;   // <nnh/texture.hpp>

inline constexpr int kMaxParticles = 8192;        ///< capacity of one emitter
inline constexpr int kMaxParticleBursts = 8;
inline constexpr int kMaxParticleKeys = 8;        ///< colour keys, size keys
inline constexpr int kMaxParticleFrames = 16;
inline constexpr int kMaxParticlePrewarm = 600;   ///< frames

enum class ParticleShape : std::uint8_t { Point, Box, Circle, Line };
enum class ParticleDirection : std::uint8_t { Cone, Outward };
enum class ParticleFrameMode : std::uint8_t { First, Random, OverLife, Rate };
enum class ParticleSpace : std::uint8_t { World, Local };
enum class ParticleOrder : std::uint8_t { OldestFirst, NewestFirst };

struct ParticleRange { float min = 0; float max = 0; };            ///< uniform between; equal = no randomness
struct ParticleTimeRange { FrameTime min = Frames{60}; FrameTime max = Frames{60}; };
struct ParticleCount { std::uint16_t min = 0; std::uint16_t max = 0; };
/// Fires at `at` after the start (and the delay), `cycles` times (0 = for ever) every `interval` (≥ 1 frame).
struct ParticleBurst { FrameTime at{}; ParticleCount count{}; std::uint16_t cycles = 1; FrameTime interval = Frames{1}; };
struct ParticleColorKey { float at = -1; Color color = colors::white; };   ///< at 0..1 of life; < 0: unused (the default)
struct ParticleSizeKey { float at = -1; float size = 1; };                ///< size: a multiplier
/// A region of the draw texture and its pivot. region.w < 0: unused (the default); an empty region: the whole texture.
struct ParticleFrame { Region region{0, 0, -1, -1}; Vec2 pivot{}; bool center_pivot = true; };

/// Every value of an emitter (FR-2): plain, trivially copyable, data-loadable (engine/core/schemas). The default is a
/// valid emitter: white 1-pixel particles at a point, 60 frames of life, no motion. Used keys and frames come first.
struct ParticleDesc {
    // Emission (FR-4, FR-5)
    float rate = 0;                    ///< particles per second, 0..491 520
    FrameTime delay{};                 ///< before the first emission
    FrameTime duration{};              ///< 0: no end
    bool looping = false;              ///< at the end of `duration` start again (bursts re-armed)
    ParticleBurst bursts[kMaxParticleBursts] = {};
    // Shape, in local space (FR-8)
    ParticleShape shape = ParticleShape::Point;
    Vec2 box{};                        ///< Box: size, centred
    bool edge = false;                 ///< Box: on the outline only
    float radius = 0;                  ///< Circle: outer radius
    float inner_radius = 0;            ///< Circle: a ring from here to `radius` (equal: on the circle)
    float arc_start = 0;               ///< Circle: radians clockwise from +x
    float arc = kFullTurn;             ///< Circle: the arc's width
    float length = 0;                  ///< Line: along local x, centred
    // Direction and speed (FR-9)
    ParticleDirection direction = ParticleDirection::Cone;
    float angle = 0;                   ///< Cone: radians clockwise from +x (−kQuarterTurn = up)
    float spread = 0;                  ///< Cone: full width (kFullTurn: every direction)
    ParticleRange speed{};             ///< px/s, may be negative
    float inherit_velocity = 0;        ///< 0..1 of the emitter's own velocity (world space)
    // Motion (FR-11)
    Vec2 gravity{};                    ///< px/s²
    bool world_gravity = false;        ///< in world space even for a turned or local-space emitter
    float damping = 0;                 ///< 1/s, 0..60: each frame keeps 1 − damping/60 of the velocity
    ParticleRange rotation{};          ///< initial, radians
    ParticleRange spin{};              ///< rad/s
    ParticleTimeRange lifetime{};
    // Appearance (FR-12, FR-13)
    Color color_min = colors::white;   ///< start colour: a random point between the two
    Color color_max = colors::white;
    ParticleRange size{1, 1};          ///< start size: a scale of the frame, or the side in pixels without a texture
    ParticleColorKey color_keys[kMaxParticleKeys] = {};
    ParticleSizeKey size_keys[kMaxParticleKeys] = {};
    ParticleFrame frames[kMaxParticleFrames] = {};
    ParticleFrameMode frame_mode = ParticleFrameMode::First;
    std::uint8_t frame_repeats = 1;    ///< OverLife: times through the frames, 1..255
    float frame_rate = 0;              ///< Rate: frames per second, 0..600
    // Space, order, determinism (FR-14, FR-15, FR-18, FR-7)
    ParticleSpace space = ParticleSpace::World;
    ParticleOrder order = ParticleOrder::OldestFirst;
    std::uint32_t seed = 1;
    std::uint16_t prewarm = 0;         ///< frames simulated at creation and restart (≤ 600; cost frames × alive)
};

struct ParticleDrawOptions {
    int layer = 0;                     ///< 0..15, sorted with sprites and tile layers (D26)
    bool ignore_camera = false;
    Color tint = colors::white;        ///< multiplied into every particle's colour
    float opacity = 1;
};

struct ParticleStats {
    int live = 0, peak = 0, capacity = 0;
    std::uint32_t emitted = 0;         ///< particles created since creation or restart
    std::uint32_t dropped = 0;         ///< asked for while full
    int drawn = 0, culled = 0;         ///< last frame (both screens, every draw)
};

/// One live particle, as drawn (FR-22): in the emitter's space (world, or local for a local-space emitter).
struct ParticleView {
    Vec2 position{}; Vec2 velocity{};  ///< velocity in px per frame (× 60 = px/s)
    float rotation = 0; float size = 0; Color color{}; int frame = 0;
    FrameTime age{}; FrameTime lifetime{};
};

class ParticleEmitter {
public:
    /// One block of `tag` (default "particles"): 48 B per particle + ≈ 2.1 KB. InvalidArgument (a description out of
    /// range, `capacity` outside 1..8192), OutOfMemory. Runs `prewarm` frames.
    static Result<ParticleEmitter> create(const ParticleDesc& desc, int capacity, MemTag* tag = nullptr);
    /// The same checks without creating (a preset loaded at run time). `why` names the first bad field.
    static Result<void> check(const ParticleDesc& desc);
    ParticleEmitter() = default;       ///< empty: commands assert and do nothing, queries answer zero
    ~ParticleEmitter();                ///< unbinds, forgets this frame's batches, frees the block
    ParticleEmitter(ParticleEmitter&&) noexcept;  ParticleEmitter& operator=(ParticleEmitter&&) noexcept;
    ParticleEmitter(const ParticleEmitter&) = delete;  ParticleEmitter& operator=(const ParticleEmitter&) = delete;
    bool valid() const;

    const ParticleDesc& desc() const;
    /// Load-time cost (tables rebuilt). Live particles keep their drawn values; a bad description asserts and
    /// changes nothing. Capacity can't change.
    void set_desc(const ParticleDesc& desc);
    void set_seed(std::uint32_t seed); ///< later emissions and restarts use it

    /// Where the emitter is (FR-14). `jump`: no sub-frame spreading or inherited velocity across this move.
    /// Non-finite values assert and are ignored.
    void set_transform(const Transform2D& t, bool jump = false);
    void set_transform(const WorldTransform2D& t, bool jump = false);   ///< a node's world (spec 022)
    void set_position(Vec2 position, bool jump = false);                ///< rotation and scale kept
    const WorldTransform2D& transform() const;

    void start(); void stop(); void clear(); void restart();
    bool emitting() const; bool finished() const; int alive() const;
    /// On demand (FR-6), even when stopped; age 0. Returns the particles created (the rest dropped and counted).
    int emit(int count);
    int emit_at(Vec2 point, int count);                   ///< the point in the emitter's space; axes kept
    int emit_at(const WorldTransform2D& at, int count);   ///< another place and orientation

    /// One 60 Hz frame (FR-17). No allocation, no game code, no log.
    void update();
    void bind(Scene& scene);           ///< D30: BeforeUpdate runs update(), AfterExit clears
    void unbind();

    /// One draw-list command (FR-19); read at submit. Without a texture: solid squares of `size` pixels.
    void draw(Screen screen, const ParticleDrawOptions& options = {}) const;
    void draw(Screen screen, const Texture& texture, const ParticleDrawOptions& options = {}) const;

    int capacity() const;
    Rect bounds() const;               ///< of the live particles' positions after the last update/emission, in world space
    ParticleView particle(int index) const;   ///< 0 = oldest; asserts outside 0..alive()−1
    ParticleStats stats() const;
private:
    detail::ParticleBlock* block_ = nullptr;
};
} // namespace nnh

// <nnh/gfx.hpp>: gfx::Stats gains (at its end) particles_last_frame, max_particles, dropped_particles.
// <nnh/app.hpp>:  GfxConfig gains `int max_particles = 0;` after max_tiles (0..65535).
```

Exact names may still change while building; stage notes record any change.

### Events introduced

None (FR-23, constitution IV). The docs show a game publishing its own `ExplosionOver` when `finished()` turns true.

### Semantics fixed by the plan

**Description checks** (`particle_desc_check.cpp`, shared by `create`, `check`, `set_desc`): every float finite; rate
0..491 520; `delay`, `duration`, lifetimes, burst times and intervals valid `FrameTime`s, lifetime min 1 tick and
min ≤ max; burst counts min ≤ max ≤ 8192, interval ≥ 1 frame when cycles ≠ 1; box ≥ 0; 0 ≤ inner_radius ≤ radius;
arc 0..kFullTurn; length ≥ 0; spread 0..kFullTurn; inherit 0..1; damping 0..60; colour and size keys: used ones first
(an unused key followed by a used one is an error), `at` in 0..1 and ascending (equal allowed: a step); frames: used
ones first, regions with x, y ≥ 0 (texture bounds are checked at draw: a frame outside the texture asserts and the
batch is skipped); `frame_repeats` 1..255, `frame_rate` 0..600, prewarm ≤ 600; enums in range. `check` names the first
bad field (`"color_keys[2].at: keys must be ascending"`).

**Derived at creation and `set_desc`** (`particle_derived.cpp`): per-frame units (speed/60, gravity/3600, spin/60,
keep = 1 − damping/60, rate in 16.16 per second and `1/rate`), the cone's limits, the ring mixture probability
`2·inner/(inner + outer)`, colour and size segments (≤ 8: start, `1/length` or 0, start and end values as floats; no
key or one key = one constant segment over [0, 1]), a 256-entry byte table "segment at the start of bucket b" for each
curve, the frame count `n`, the unified frame constants (below), flags choosing the update and expansion variants, the
largest size factor (for culling), the shape sampler.

**`update()`** — in this order:
1. **Clock**: if started and not ended: the delay counts down first; then `clock += 4096` ticks; with a duration, at
   its end either `clock −= duration` and the bursts re-arm (looping) or the emission ends.
2. **Motion** of the existing particles (`emitter_update.cpp`, variants `<spin, drag>`), for each in order:
   `age += 4096`; if `age ≥ lifetime` it dies (not copied); else `v += g; v ×= keep` (drag variant, then the velocity
   flush); `p += v`; `rotation += spin` (spin variant); the record is stored at the write index (and its cold record
   moved when the index lags); the position's x and y enter the bounds as integer keys of their bits. Stable: emission
   order kept. `g` is the description's gravity mapped once per update — world space: through the transform's axes
   (`transform_direction`), or as is with `world_gravity`; local space: as is, or through the inverse axes with
   `world_gravity`.
3. **Rate births**: the accumulator adds the 16.16 rate; each crossing of `60 × 65536` is one particle at frame
   fraction `f = (threshold·(j+1) − a₀) × (1/rate)`; world space: born on the transform interpolated at `f` between the
   previous and the current (origin and drawing values lerped, the map rebuilt with one sine only when the rotation's
   bits differ; `f = 1` uses the current map exactly), then pre-aged by `r = 1 − f`: `p += v·r`, `age = round(r ×
   4096)` ticks (FR-10). Local space: born at age 0, no spreading (the frame moves with the emitter).
4. **Bursts** due in this frame's clock interval (at most one firing per burst per frame): count from the hash of
   (seed, burst, cycle), born on the current transform at age 0.
5. The previous transform becomes the current one (and the emitter velocity for `inherit_velocity` is the origin's
   change since the last update; 0 after a `jump`).

**Births** (`emitter_emit.cpp`, one inline `birth()` used by rate, bursts, `emit`, `emit_at`, prewarm): the emission
number `n` is taken (dropped particles take theirs too); if full, the particle is dropped and counted (first drop:
one Debug/Development warning per emitter, never in Release); else values in property order: position on the shape
(Point; Box area or edge by perimeter; Circle disc/ring/arc area-uniform; Line), direction (cone `angle + (u − ½) ×
spread`, or outward: the sampled circle angle, or the normalised point for box and line, the cone for a point or the
centre), speed, velocity `= dir × speed` mapped through the birth map (+ `inherit × emitter velocity` in world space),
rotation (+ the drawing rotation by the mirror rule in world space), spin, lifetime (integer ticks `min + ((span + 1) ×
h) >> 32`), `1/lifetime`, start colour (one value lerps every channel between `color_min` and `color_max`, rounded),
start size, the frame byte (Random: `h × n >> 32`; otherwise 0). Positions are mapped through the birth map (world
space) or stay local.

**Appearance** (`particle_appearance.hpp`, inline, used by `particle()` and the expansion): `t = age × (1/lifetime)`
(float, < 1); segment `s = table[(int)(t × 256)]`, then while `t ≥ start[s+1]` `s++` (≤ 7 steps); `frac = (t −
start[s]) × inv_len[s]`; each colour channel `c = (int)(c₀ + (c₁ − c₀) × frac + 0.5)`, then × the start colour channel
by `mul8(a, b) = round(a·b/255)` in integers (exact); size `= start_size × (s₀ + (s₁ − s₀) × frac)`. Frame: one
branch-free rule for every mode — `m = base + (int)(age_f × kA + t × kB)` with `kA` (Rate: frames per tick) or `kB`
(OverLife: `n × repeats`), the other 0, `base` the cold frame byte (Random) or 0, then `frame = m − n × ((m ×
recip_n) >> 32)` (a 32-bit reciprocal, exact over the whole domain: m < 2²⁴, proven exhaustively by a test); OverLife
clamps `m` to `n × repeats − 1`.

**Lifecycle**: `start()` (re-arms an ended emission: clock and bursts from 0; delay again), `stop()` (no more rate or
burst emission; live particles finish), `clear()` (alive 0, bounds empty; clock and random sequence unchanged),
`restart()` = clear + clock, accumulator, bursts and emission number to 0 + previous transform = current + start +
prewarm (the same calls afterwards give the same bits). `emitting()` = started and not ended. **Emission over** =
not started, or ended (duration without looping), or (rate 0, every burst's cycles done, not looping). `finished()` =
emission over and `alive() == 0`. Prewarm runs `prewarm` updates on the current transform (no movement, no jump).

**Transforms**: stored as a `WorldTransform2D` (+ the previous one). `set_transform(Transform2D)` = `to_world`;
`set_position` keeps the axes and drawing values (a pure translation, no sine). Non-finite values (integer test) assert
and are ignored. Shear: points exact through the map; sub-frame interpolation and drawing use the drawing values
(exact without shear, spec 022's rule).

**Local space drawing**: position `= transform_point(map, p)`, rotation `= R + (mirror ? −r : r)`, scale `= drawing
scale × size` (signs flip the sprite, as `gfx::transformed`; tested equal to it). World space: position `p`, rotation
`r`, scale `(size, size)`.

**Expansion** (`particle_expand.cpp`, variants `<textured, local>`): per batch, once: the view (camera position,
screen size × `1/zoom`, or the screen when ignoring the camera), the frames' regions and pivots resolved against the
texture (empty → whole texture; `center_pivot` → region centre), the frame reach `max(|dx| + |dy|)` over pivot-to-corner
offsets (an L1 bound of the rotated quad; squares: their side), the emitter test (world bounds grown by `largest size
factor × reach`); per particle in draw order (oldest or newest first): the reach test on integer keys (culled
counted), appearance, then the item through `gfx/sprite_place.hpp` (Sprite: the same arithmetic as `DrawList::build`
for a `gfx::draw` at that position with region, pivot, scale, rotation, tint `= mul8(particle colour, draw tint)` with
alpha `× opacity` rounded as `gfx::draw` does; FillRect: as `gfx::fill_rect({p − s/2, s, s})`). Squares don't rotate
(FillRect has no rotation; documented). Stops at the chunk's end (`next` = the resume index) or the budget (the rest
dropped and counted in gfx's stats). Counts drawn and culled into the emitter's stats (reset when the frame number
changes).

**Scene binding** (`emitter_scene.cpp`, D30 unchanged): the block holds a `SceneAttachment`; `BeforeUpdate` →
`update()`, `AfterExit` → `clear()`. A bound emitter's rate births follow the transform the game set last frame
(documented: unbind for the exact order).

**Misuse (FR-25)** — assert in Debug/Development, do nothing (or clamp) in Release: a bad description to `set_desc`;
non-finite transforms or points; `emit` with a negative count (0); any command on an empty or moved-from emitter;
`draw` outside a frame, with `max_particles == 0` (Release: one warning per init), with more than 32 batches in a frame
(Release: dropped, warned), with a frame region outside the texture; `particle(i)` out of range (a zero view); `bind`
twice (ignored). Never undefined behaviour (ASan/UBSan, fuzz).

### D29 amendment (presets, FR-21, AC-3)

The engine ships `engine/core/schemas/particles_schema.cpp` (`NNH_DATA_ENUM` for the five enums, `NNH_DATA_STRUCT`
for `FrameTime` (`ticks`), the ranges, bursts, keys and frames (`Region` too), `NNH_DATA_TYPE(ParticleDesc,
"particles", …)` with `.min`/`.max` from the checks above). It is **not** in `nnh_core`: a game lists
`${NNH_PARTICLES_SCHEMA}` (a CMake variable set by `cmake/nnh_data.cmake`) in `nnh_add_game(... DATA_SCHEMA ...)`;
D29's pipeline compiles it into the game and its schema program like any schema source (absolute paths already work).
Two generic, opt-in field rules are added to D29, each written into the schema description **only when used**, so no
existing type's description, fingerprint or file changes (`data check: PASS checksum 0xD5BD3020` holds):

- `.ascending = true` on a number field that is an array/list item or a member of a struct item: each item's value is
  not smaller than the previous item's. Converter message: `file:line: color_keys[2].at must not be smaller than the
  previous item's (0.25 < 0.5)`; core's shared validator refuses such a binary as `Corrupted`.
- `.up_to = true` on a `T[N]` field: a file writes 1..N items; the others keep the struct's default member values (the
  binary carries the written count, as a list's; core writes only those items). Without it, `T[N]` stays "exactly N".

The description's keys and frames use them (`at`/`region` defaults mark unused items). `FrameTime` values are written
in ticks in files (4096 per frame; the docs give a table); a "seconds or frames" data kind is a follow-up (decision 14).
`create`/`set_desc` re-check every loaded description. Files: `include/nnh/data.hpp` (`FieldOptions`),
`detail/data_schema.hpp` (`FieldInfo` gains two bools in its padding), `detail/data_tables.hpp`, `src/data/schema_write.cpp`,
`schema_check.cpp` (the rule's valid places), `data_validate.cpp`, `data_apply.cpp`, `tools/assetc/src/schema_read.cpp`,
`data_values.cpp`/`data_writer.cpp`; tests in `tests/data_assets/` and `tools/assetc/tests/`, fixtures
`tests/data/data_assets/bad/{not_ascending,too_many_up_to}.enemy` with their messages.

### gfx additions

- `gfx/particle_batches.{hpp,cpp}` (new): `ParticleBatchRecord`, `ParticleExpandRequest {source, screen, view, camera,
  zoom, texture handle/size or none, tint, layer, first, out, capacity}`, `ParticleExpandResult {written, culled, next}`,
  the table (32), the scratch (128 items), the budget, `gfx_record_particles`, `gfx_forget_particles`, the submit loop,
  the ops and their registrar; compiled with `-ffp-contract=off`.
- `gfx/sprite_place.hpp` (new, inline): `snap`, `place_sprite(...)`, `place_rect(...)` moved out of `draw_list.cpp`'s
  anonymous namespace; `draw_list.cpp` includes it (3ds-dev `draw_list.o` byte-identical, T1.2).
- `gfx_frame.cpp`: `g_gfx_particle_ops` (null), the split dispatch (`< kMaxTileBatches` → tiles, else ops), and four
  null-checked calls (init, free, end of frame, stats in `draw2d.cpp`). `gfx_state.hpp`: `splits[kMaxTileBatches +
  kMaxParticleBatches]` (96: a screen may hold 64 tile and 32 particle batches). `draw_list.hpp`: `tile_batch`'s comment
  (≥ 64: a particle batch). `debug/overlay.cpp`: `particles %d/%d dropped %d` under the tiles line when the cap > 0.
- 3DS (`app_3ds.cpp`): `C2D_Init` objects += `max_particles`. PC, fake: nothing.

### Data and memory

| Item | 3DS | 64-bit PC | Notes |
|------|-----|-----------|-------|
| hot record | 32 B | 32 B | px, py, vx, vy, rotation, spin (floats), age, lifetime (u32 ticks); 32-byte aligned |
| cold record | 16 B | 16 B | 1/lifetime, start size (floats), start colour (RGBA8), frame (u8), 2 spare |
| **per particle** | **48 B** | 48 B | FR-24's expectation; 1024 particles = 48 KB |
| per emitter | ≈ 2.1 KB | ≈ 2.2 KB | header ≈ 220 B (state, clock, accumulator, counters, transforms now/previous 72 B, scene node, stats), description ≈ 860 B, derived ≈ 1 KB (segments, two 256-byte bucket tables, constants) |
| `ParticleEmitter` | 4 B | 8 B | the block pointer |
| gfx, `max_particles > 0` | ≈ 8.2 KB | ≈ 9.2 KB | 32 batch records (≈ 28 B) + 128-item scratch (`DrawItem2D` 56 B / 64 B) |
| gfx, every game | +260 B `.bss` | +264 B | 32 more split points (8 B) and the ops pointer |
| 3DS citro2d | 192 B × `max_particles` (Linear) | — | 6 vertices of 32 B per quad, as for `max_draws`/`max_tiles` |
| stack | < 400 B | | update, births, expansion; nothing per frame on the heap |

Examples: the platformer's three emitters (64 + 64 + 32) ≈ 14 KB + `max_particles = 256` (8.2 KB + 48 KB Linear); the
example ≈ 200 KB of emitters (the bench's 1024-particle emitter included) + `max_particles = 4096`.

### ARM11 cost plan (AC-7)

Lessons from specs 019–023 on the Old 3DS (ARM11 268 MHz, VFPv2 without NEON, 16 KB L1D with 32-byte lines, no L2, no
hardware prefetcher, ≈ 110 cycles per miss; instruction counts were 1.3–2× low): a sequential walk with `pld` costs ≈
40 cycles per line read, ≈ 60 for a line read and written back; random ≈ 110; mixed code ≈ 1.6–2.2 cycles per
instruction misses aside; VFP11 `vadd`/`vmul` latency 8 cycles (dependency chains matter: no FMA under
`-ffp-contract=off`). The upper end of each estimate is multiplied by 1.3. Built in:

1. **One line per particle per update**: the 32-byte hot record, `pld` four records (128 B) ahead; cold records are
   touched only by births and by compaction moves (half a line per survivor after the first death).
2. **Specialised loops**: update `<spin, drag>`, expansion `<textured, local>` chosen once per call; constant curves
   are one segment (same code, no branch); the frame rule is branch-free; births call the shape sampler through a
   per-emitter function pointer (one predicted indirect call per birth).
3. **No `vmrs`**: bounds and culling compare integer keys of float bits (`k = b ^ ((b >> 31) & 0x7FFFFFFF)`, signed);
   the velocity flush and finiteness are exponent tests.
4. **No `__aeabi_*`**: no integer division or modulo (reciprocals, masks), no `memcpy` (records copied as words: T7.6
   checks GCC made no call), no `floorf` call in the update (the expansion's `snap` is the shared `std::floor`, as the
   sprite path; T7.6 counts it; an exact integer floor is mitigation (d)).
5. **Births**: one `mix` per value (6 instructions), `sin_cos_inline` (≈ 50 instructions, two Horner chains of 8
   dependent VFP operations), the birth map skipped for a pure translation (`emit_at(point)` on an unturned emitter).
6. **Expansion into L1**: a 7 KB scratch written and read back at once by `draw_2d` (the sprite path writes a 72 B
   command, sorts it twice, builds a 56 B item and reads it back: ≈ 10 lines per sprite, ≈ 4 µs of misses).
7. **The platform's per-item cost is the same for a particle and a sprite** (subtexture with 4 `vdiv`, tint, citro2d's
   own `sinf`/`cosf` when rotated, 192 B of vertices): calibrated from D26's measurements, a sprite ≈ 11.5–12.8 µs all-in
   (1300 at 60 Hz) and a tile quad ≈ 4.3 µs (6 layers + 300 sprites = 4 layers + 600 sprites), so the platform's share
   of an unrotated sprite is ≈ 5 µs and the core's (record, sort, build, misses) ≈ 5.5–7.8 µs.
8. **FPSCR**: round-to-nearest, flush-to-zero on the 3DS; the velocity flush keeps every value normal.

Estimates (1 ms = 268 000 cycles):

| AC-7 workload | Hot path | Estimate (upper × 1.3) | Target | Verdict |
|---------------|----------|------------------------|--------|---------|
| `update()`, 1024 live in one emitter (rate, gravity, drag, spin, 4-key gradient, 3-key size curve, frames over life) | ≈ 45 instructions and ≈ 35–45 cycles of VFP chains per particle; 1 hot line (≈ 60 cycles) + ½ cold line moved (≈ 30); ≈ 17 births per frame × ≈ 600 cycles; per-call ≈ 1 k cycles (appearance is not evaluated) | 0.45–0.65 ms (0.85) | < 1.2 ms | expected PASS |
| the same in 16 emitters of 64 | as above + 16 × (≈ 300 instructions of clock, bursts, transforms, bounds + ≈ 10 header misses) | 0.55–0.8 ms (1.05) | < 1.4 ms | expected PASS |
| emitting 512 in one frame (32 `emit_at` × 16, circle area, cone 2π, every range random) | per birth ≈ 300 instructions (11 hashed values ≈ 70, 2 `sin_cos` ≈ 100 + ≈ 180 cycles of chains, radius, velocity, colour, size, frame, `1/lifetime` (`vdiv`), writes, bounds) + ≈ 60 cycles of fresh lines → ≈ 550–700 cycles; 32 calls × ≈ 300 cycles | 1.05–1.35 ms (1.75) | < 1.0 ms | **likely over** (risk 1) |
| drawing 512 visible (8×8 frames, rotated, tinted, top screen): per particle vs a `gfx::draw` sprite with the same options | particle: expansion ≈ 180–220 instructions (t, two segments, 4 channels, `mul8` ×4, size, frame, region, cull, `place_sprite` with 2 `floorf`) + 1.5 lines prefetched ≈ 1.3–1.7 µs, + the platform's ≈ 6.3–6.8 µs; sprite: core ≈ 5.5–7.8 µs + the same platform share | ratio 0.52–0.66 (0.70) | ≤ 0.75 | expected PASS, tight |
| 512 simulated and drawn + 300 sprites, 600 frames | ≈ 0.35 ms update + 512 × 7.6–8.5 µs + 300 × 11.5–12.8 µs + ≈ 1.5 ms frame base | worst frame 9.2–11.7 ms (13.5) | no missed frame | expected PASS |
| an emitter of 512 entirely off-screen | its update + one bounds test at submit (≈ 40 instructions) | update + < 1 µs | ≤ its update | PASS by construction |

The targets stay as written; T7.6 checks the disassembly and the miss model against this table before the console,
T10.2 measures, a missed target is optimised first (risk 1's list, no result may change), then the budget is set from
the measurement with ≈ 25 % margin and written in the spec and the docs with the numbers.

### Self-check and bench (AC-2, AC-6, AC-7)

- **Self-check** (`examples/particles/self_check.{hpp,cpp}`, pure, compiled into the example and `nnh_tests`): 8
  emitters, 3600 frames, every call from a fixed table or FNV-1a of (seed, frame, salt): E1 point, rate 120/s, cone ±30°
  up, gravity, damping, world space on a moving and turning transform (a circle path, rotation `frame × kQuarterTurn /
  64`), inherit 0.5, 4 colour keys, 3 size keys, frames over life ×2; E2 box area, rate 600/s, capacity 256 (kept full:
  drops counted); E3 box edge, 3 bursts (cycles 4, interval 30 frames, counts 8..16), duration 120 frames, looping; E4
  circle ring with an arc, outward, one burst, `restart()` every 300 frames; E5 line, local space, rotated and mirrored
  every 400 frames (`scale (−1, 1)`), spin, frames at a rate; E6 circle area, `emit_at` at seeded points every 7 frames
  (1..20 particles, sometimes through a transform), random frames, `stop()`/`start()` every 250 frames; E7 point,
  prewarm 120, local space with `world_gravity`, `set_desc` to another description at frame 1800; E8 delay 60, duration
  600, no loop (it finishes), `restart()` at 2400, a `jump` every 500 frames. Every 600 frames an FNV-1a over each
  emitter's `alive`, `emitted`, `dropped`, `finished()` and every live particle's view (position, velocity, rotation,
  size bits, colour, frame, age, lifetime). Goldens from pc-debug in `kParticlesCheckGolden`/`kParticlesCheckpoints`,
  equal in pc-release, GCC 13/14, clang 18 and the console. The example runs 10 script frames per display frame (≈ 6 s).
- **Bench** (`examples/particles/bench.{hpp,cpp}` pure workloads, `bench_scene.{hpp,cpp}` timing them, 300 frames
  each, average and worst, only the measured calls in the profile scope): `update1024` (capacity 1024, rate 1000/s,
  lifetimes 0.9–1.1 s, prewarm 120, the features of AC-7), `update16x64`, `emit512` (an 8192 emitter cleared outside the
  scope), `draw512` (512 frozen visible particles drawn each frame) and `sprite512` (512 `gfx::draw` with the same frames,
  rotations and tints): cost per frame = the scene's own scope + (`gfx submit` − the base run's average `gfx submit`), as
  plan 023 measures; `frame` (512 simulated and drawn + 300 16×16 sprites, 600 frames, missed frames from `time::delta()`
  > 20 ms); `offscreen` (512 particles outside the view: `gfx submit` − base, drawn 0, culled 512). The `gfx submit`
  scope is plan 023's (Development only), see "Merge notes". Lines: `particles bench: <name> avg X ms worst Y ms
  (target < Z ms) PASS|FAIL`, `particles bench: draw particle P us sprite S us ratio R (target <= 0.75) PASS|FAIL`,
  `particles bench: frame worst W ms missed M of 600 PASS|FAIL`, `particles bench: offscreen submit X ms culled 512
  PASS|FAIL`, then `particles bench: PASS` or `particles bench: FAIL (<names>)`; a Debug build logs `not judged (Debug)`.

### The example `examples/particles` (AC-4)

Original art (`art/make_art.py` → `assets/art/particles.png`, 64 × 64: spark, smoke puff, four flame frames, rain
streak, ember, star, the ship) and presets (`assets/fx/embers.particles`, the gallery's data-loaded effect;
`assets/fx/sparks.particles`, equal to the self-check's E6 description). `DATA_SCHEMA ${NNH_PARTICLES_SCHEMA}`;
`AppConfig::gfx = {.max_draws = 4096, .max_tiles = 512, .max_particles = 4096}`. **Top screen** (the gallery): a torch
(local space on a swaying transform, rate, gradient, flame frames over life), chimney smoke (rate, damping, growing
size), rain (box, world space, high rate) falling behind or in front of a code-built tile band (`Tilemap::create`, one
16×16 layer at draw layer 5; rain at 4 or 6), the ship (Circle Pad: an exhaust trail in world space with sub-frame
spreading and inherit velocity), an explosion (A: bursts outward from a ring, one-shot), texture-less squares (a
fountain), embers (the preset). **Bottom screen**: the selected effect's name, the emitters' stats, `gfx::stats()`'s
particle line; a **touch** bursts the selected effect at the touched point (`emit_at` on that effect's bottom-screen
emitter, drawn on the bottom screen). Controls (Old 3DS buttons): L/R select the effect, A explosion, X world/local
space (torch and ship; `restart()` after the switch), B rain behind/in front of the tile band, D-Pad Down smoke
stop/start, SELECT self-check, Y bench, START quit. Log lines: `particles: showcase`, `effect: <name>`, `burst: <name>
<n> at <x>,<y> live <live>`, `done: <name> live 0 finished` (once when a touch burst's emitter finishes), `explosion:
<n> live <live>`, `space: world|local`, `layer: behind|front`, `smoke: stopped|started`. Screenshots by
`PARTICLES_SHOT=gallery|trail|explosion|touch|local|world|full` (fixed inputs, fixed frame).

### The platformer (AC-5)

New `examples/platformer/level_effects.{hpp,cpp}`: `LevelEffects` owns three emitters outside the world, all
texture-less squares on draw layer `kPlayer`: **dust** (capacity 64: a burst of 10 at the feet when the player lands,
6 behind the feet when it starts running — on the ground and `|vx|` crossing 0.5 px per frame), **puff** (64: 8 where
a bullet stops on a wall, along the wall's normal; 12 at the player's centre when `hits()` grew), **flash** (32: 6 along
the barrel when a turret fires, from spec 022's muzzle point `transform_point(barrel world map, kMuzzle)` with the
barrel's world rotation). `update(const EffectsInput&)` reads (never writes) the player, the bullet pool (`stuck ==
kStuckFrames`: stopped this frame), the turrets (`cooldown == kFirePeriod`: fired this frame) and the hierarchy
(stored worlds, after spec 022's pass), emits, then updates the three emitters; `draw()` draws them on the top screen;
`clear()` on restart. Seeds are fixed. Nothing it touches is in `ReplaySample` (box, camera, touching, coins, turret
state, entity count, handle hash), and it changes no entity, tween, pool or turret value, so ctest
`platformer_replay_matches_script` and `examples/platformer/tests/level.nnhs` pass **with the lines in the tree**
(spec 022's), unedited. `LevelPlay` hunks: one include and one member (`LevelEffects effects_;` after `sparkles_`) in
the header; in the source one statement at the end of `update()` (after `camera_.apply();`), one in `restart()`
(after `turrets_.restart();`), one in `draw()` (after `draw_sparkles();`). `main.cpp`: `.max_particles = 256`. The
bench, gate, zoom and flip demos are untouched (their scenes have no `LevelPlay`).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/particles.hpp` | new | the public API above (≈ 260 lines with comments) |
| `engine/core/src/particles/particle_block.hpp` | new | block layout (header, hot/cold records, derived), access helpers, the `particles` tag and log channel |
| `engine/core/src/particles/particle_desc_check.{hpp,cpp}` | new | the description checks and `check()` |
| `engine/core/src/particles/particle_derived.{hpp,cpp}` | new | per-frame units, segments, bucket tables, frame constants, variant flags, shape sampler choice |
| `engine/core/src/particles/particle_random.hpp` | new | the counter-based generator (inline, `core/mix32.hpp`) |
| `engine/core/src/particles/particle_shapes.{hpp,cpp}` | new | shape samplers (point, box, edge, disc, ring, arc, line), directions, the fixed reciprocal square root |
| `engine/core/src/particles/particle_appearance.hpp` | new | `t`, colour and size segments, `mul8`, the frame rule (inline) |
| `engine/core/src/particles/emitter_create.cpp` | new | create, check, move, destroy, `set_desc`, `set_seed`, prewarm |
| `engine/core/src/particles/emitter_clock.cpp` | new | clock, delay, duration, looping, bursts, rate accumulator, lifecycle, `finished()` |
| `engine/core/src/particles/emitter_emit.cpp` | new | `birth()`, `emit`, `emit_at`, sub-frame transforms, the full case and its warning |
| `engine/core/src/particles/emitter_update.cpp` | new | `update()`'s order, the motion variants, compaction, bounds, the velocity flush |
| `engine/core/src/particles/emitter_transform.cpp` | new | transform setters, `transform()`, `bounds()` |
| `engine/core/src/particles/emitter_queries.cpp` | new | `particle()`, `stats()`, `alive()`, `capacity()` |
| `engine/core/src/particles/emitter_scene.cpp` | new | D30 binding |
| `engine/core/src/particles/emitter_draw.cpp` | new | `draw()` → `gfx_record_particles` |
| `engine/core/src/particles/particle_expand.cpp` | new | the batch's expand function (culling, appearance, items) |
| `engine/core/src/gfx/particle_batches.{hpp,cpp}` | new | gfx's particle batches, scratch, budget, submit loop, ops registrar |
| `engine/core/src/gfx/sprite_place.hpp` | new | shared placement (`snap`, sprite and rect items) |
| `engine/core/src/gfx/draw_list.{hpp,cpp}`, `gfx_frame.cpp`, `gfx_state.hpp`, `draw2d.cpp`, `debug/overlay.cpp` | changed | the shared placement, dispatch, splits, stats, overlay line (see "gfx additions") |
| `engine/core/include/nnh/gfx.hpp`, `app.hpp` | changed | three `Stats` fields; `GfxConfig::max_particles` |
| `engine/core/src/core/mix32.hpp`; `src/camera/camera_shake.cpp` | new / changed | the finalizer moved (bits unchanged) |
| `engine/core/schemas/particles_schema.cpp` | new | the opt-in preset schema |
| `cmake/nnh_data.cmake` | changed | `NNH_PARTICLES_SCHEMA` |
| D29 files (see "D29 amendment") | changed | `.ascending`, `.up_to` |
| `engine/platforms/3ds/src/app_3ds.cpp` | changed | `C2D_Init` sizing |
| `engine/core/CMakeLists.txt` | changed | one `_nnh_particles_sources` block (with `gfx/particle_batches.cpp`, `-ffp-contract=off`) |
| `tests/particles/particles_fixture.hpp`, `test_desc_check.cpp`, `test_create.cpp`, `test_random.cpp`, `test_rate.cpp`, `test_bursts.cpp`, `test_lifecycle.cpp`, `test_prewarm.cpp`, `test_shapes.cpp`, `test_directions.cpp`, `test_subframe.cpp`, `test_motion.cpp`, `test_order_full.cpp`, `test_appearance.cpp`, `test_frames.cpp`, `test_space.cpp`, `test_set_desc.cpp`, `test_queries.cpp`, `test_scene_binding.cpp`, `test_particle_misuse.cpp`, `test_particle_cost.cpp` | new | FR-1–FR-18, FR-22–FR-25 |
| `tests/particles/test_expand.cpp`, `test_draw.cpp`, `test_particle_budget.cpp`, `tests/gfx/test_sprite_place.cpp`, `tests/camera/test_mix32.cpp` | new | FR-19, FR-20, the shared helpers |
| `tests/particles/test_presets.cpp`, `tests/data_assets/test_ascending.cpp`, `test_up_to.cpp`, `tests/data/particles/` (`good/*.particles`, `bad/*.particles` + messages, `schema.nnhschema`), `tests/data/data_assets/bad/*` | new | FR-21, AC-3, the D29 rules |
| `tests/particles_example/test_self_check.cpp` | new | AC-2 golden |
| `tests/fuzz/fuzz_particle_ops.cpp`, `libfuzzer_particle_ops.cpp`, `test_fuzz_particle_ops.cpp` | new | random descriptions and operation streams (X, FR-25) |
| `tests/platformer/test_level_effects.cpp` | new | AC-5: effects emit on landing, running, wall stops, hits, firing; nothing else changes |
| `tests/CMakeLists.txt` | changed | one 024 block + ctests (merge notes' anchors) |
| `examples/particles/` | new | `main.cpp`, `demos.hpp`, `effects.{hpp,cpp}` (gallery descriptions), `gallery.{hpp,cpp}`, `ship.{hpp,cpp}`, `touch_panel.{hpp,cpp}`, `hud.{hpp,cpp}`, `showcase_scene.{hpp,cpp}`, `self_check.{hpp,cpp}`, `check_scene.{hpp,cpp}`, `bench.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `art/make_art.py`, `assets/…`, `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(particles)` |
| `examples/platformer/level_effects.{hpp,cpp}`, `level_play.{hpp,cpp}`, `main.cpp`, `CMakeLists.txt` | new / changed | AC-5 |
| docs (stage 11) | new / changed | `docs/api/particles.md` (new); `gfx.md`, `app.md`, `data.md`, `transform.md`, `hierarchy.md`, `animation.md`, `architecture/rendering.md`, `project-structure.md` (`engine/core/schemas/`), `decisions.md` (D39 implemented, D29 note), `patterns.md`, `docs/REFERENCES.md`, `CLAUDE.md`, `docs/ROADMAP.md`, `scripts/check_release_strings.sh` if needed |

### Platform work

- **pc:** none (SDL's `draw_2d` draws the items).
- **3ds:** `app_3ds.cpp` adds `max_particles` to `C2D_Init`'s object count. Mitigation only (risk 2): skip redundant
  `C2D_SetTintMode` calls in `draw2d_3ds.cpp`.
- **fake:** none; tests read its recorded items and calls (`T`/`t`/`B`/`b` per `draw_2d`/`draw_tiles`).

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows; VM 120: GCC 13/14, clang 18, ASan/UBSan). Statistical
checks use fixed seeds and fixed bounds (a uniform test over 10 000 values; a disc test by rings of equal area); exact
values come from hand-computed cases; the expansion is compared with `DrawList::build()`'s items for the equivalent
`gfx::draw`/`fill_rect` commands.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `test_create.cpp`: capacities 1 and 8192, 0 and 8193 refused, `OutOfMemory` (a failing tag), tag stats = header + 48 × capacity, move leaves the source empty, many emitters |
| FR-2 | `test_desc_check.cpp`: the default is valid and draws white 1-px particles for 60 frames; each check's boundary both ways with the field named; `test_set_desc.cpp` |
| FR-3 | `test_motion.cpp`: px/s and px/s² over 60 updates equal the per-frame sums; radians clockwise (−kQuarterTurn goes up); `test_lifecycle.cpp`: `Duration` and `Frames` lifetimes |
| FR-4 | `test_rate.cpp`: 2.5/s gives 25 in 600 frames and 150 in 3600 exactly; 0.5/s, 59.9/s, 491 520/s; delay, duration, looping |
| FR-5 | `test_bursts.cpp`: times, cycles (1, 4, for ever), intervals, count ranges, eight bursts, a one-shot that finishes |
| FR-6 | `test_lifecycle.cpp`: `emit`/`emit_at` on a stopped emitter, at a point and a transform, the emitter unmoved |
| FR-7 | `test_lifecycle.cpp`: stop lets particles finish, clear, restart reproduces the first run bit for bit, `finished()`; `test_prewarm.cpp`: prewarm = the same frames by hand, ≤ 600 |
| FR-8 | `test_shapes.cpp`: every shape's points inside, on the edge, in the ring, on the arc; uniformity by area (disc and ring by equal-area annuli, box edge by perimeter) |
| FR-9 | `test_directions.cpp`: cone limits, 2π spread, outward from circles, boxes and lines (unit length within 2⁻²⁰), inherit velocity, negative speeds |
| FR-10 | `test_subframe.cpp`: a fast emitter's births evenly spaced along its movement and turn, pre-aged, `jump` breaks it, local space unspread |
| FR-11 | `test_motion.cpp`: the documented order against a hand loop; exact lifetimes in updates; the velocity flush |
| FR-12 | `test_appearance.cpp`: at keys exact, between keys within 1/256 of a `double` reference (steep and flat), one/zero keys constant, `mul8` exhaustive |
| FR-13 | `test_frames.cpp`: first, random (uniform), over life with repeats, at a rate; the reciprocal rule exhaustive over its domain; `test_expand.cpp`: regions, pivots, whole texture, squares |
| FR-14 | `test_space.cpp`: world trails stay behind; local particles follow a moving, rotated, mirrored transform; gravity local/world; drawing equal to `gfx::transformed` |
| FR-15 | `test_random.cpp`: independence from other emitters, culling, drops (the same particles whatever the capacity), `restart`, seeds |
| FR-16 | AC-2's golden on WinLibs GCC, GCC 13/14, clang 18, the console; no `vmrs`-dependent or libm path (T7.6) |
| FR-17 | `test_motion.cpp`: no allocation (counter), no game code (none can be given), no log; `test_scene_binding.cpp`: updated before update, frozen under a pause scene, cleared after exit, D30 unchanged |
| FR-18 | `test_order_full.cpp`: emission order through deaths, oldest/newest first in the items, full: as many as fit, the rest dropped and counted, no assert, one warning per emitter (Debug/Development only) |
| FR-19 | `test_draw.cpp`: one command per draw, sorted with sprites and tile batches by layer and call order (the fake's calls), emitter destroyed after its draw call, moved (still drawn), destroyed texture (assert, skipped), both screens, twice; `test_expand.cpp`: items equal `DrawList::build`'s for zoom 0.5/1/2, `ignore_camera`, rotation, scale, tint, opacity, frames, squares; whole-emitter and per-particle culling |
| FR-20 | `test_particle_budget.cpp`: `max_particles` 0 asserts (Release: warn once, nothing drawn), over the cap (assert; Release drop and one warning per frame), the same drops on every system, stats fields, 33 batches, the overlay line; `hello` budget (T4.6) |
| FR-21 | `test_presets.cpp`: `good/sparks.particles` loads equal field for field to the C++ description; ctests on `bad/*.particles` (unsorted keys, 9 keys, out-of-range values, unknown shape) with `file:line` messages; D29 rules' tests |
| FR-22 | `test_queries.cpp`: description, transform, capacity, live, emitting, finished, bounds, views, stats (peak, emitted, dropped, drawn, culled) |
| FR-23 | `test_particle_cost.cpp`: `events::stats()` unchanged by every operation; the module includes no event header |
| FR-24 | `test_particle_cost.cpp`: allocation counter unchanged after creation over 100 000 operations; T7.6 (no option branch per particle, no `virtual`) |
| FR-25 | `test_particle_misuse.cpp`: every listed misuse asserts (`run_expecting_assert`) and is harmless on the Release path (pc-release) |
| FR-26 | include check (the header pulls only its six includes; no entities, hierarchy, animation, tween, tilemap, collision, data header); T8.3 probes; `hello` sizes and `nm` |
| AC-1 | all of the above, Windows and VM 120 |
| AC-2 | `tests/particles_example/test_self_check.cpp`, ctest `particles_self_check_pc`, the console's `particles: PASS` |
| AC-3 | `test_presets.cpp` (+ AC-2's script with E6's description from the preset gives the same checksum), the `particles_preset_*` ctests |
| AC-4 | PC screenshots read back (T7.2) |
| AC-5 | `test_level_effects.cpp`, ctest `platformer_replay_matches_script` and `level.nnhs --pc` with the tree's lines, a screenshot read back, T10.2 runs 2–3 |
| AC-6 | `examples/particles/tests/check.nnhs` with `--pc` (ctest `particles_check_script_pc`), then on the console |
| AC-7 | the bench lines on the console; T7.6's table |
| AC-8 | stage 0 references compared at stage ends; `hello` sizes and `nm`; probes; build times |
| AC-9 | stage 11 |

## Risks and alternatives

1. **AC-7: emitting 512 is likely over (1.05–1.35 ms, target 1.0); the draw ratio is tight (0.52–0.70, target 0.75).**
   Mitigations in order, none changing a result: (a) births in pairs (two `sin_cos` chains interleaved); (b) for a full
   turn the direction's sine from quarter turns directly (no reduction); (c) colour lerp and `mul8` with ARMv6 SIMD
   (`uqadd8`-style) on packed channels; (d) an exact integer `snap` proven equal to `std::floor` over every float of
   the domain (no `floorf` call); (e) a larger scratch (fewer `draw_2d` calls); then the contract fallback (c) of the
   spec's D39 question (`draw_particles` with compact quads and precomputed UVs); then AC-7's measurement-with-margin
   policy.
2. **citro2d state changes per `draw_2d` call**: `draw2d_3ds.cpp` sets the tint mode at the start and end of every
   call; if that flushes citro2d's vertex batch, each 128-item chunk costs two flushes. T7.6 can't see it; T10.2's
   `draw512` does. Mitigation: track the tint mode in the 3DS context (set only on change, restored at `end_frame`) —
   a backend change that helps sprites and tiles too, checked by every screenshot.
3. **The D29 amendment touches spec 014's pipeline**: existing descriptions, fingerprints and files must not change —
   guarded by `data check: PASS checksum 0xD5BD3020 (… index 595 bytes)`, the fixture ctests and a byte comparison of
   the example's schema description before and after (T5.4).
4. **`hello` grows** by the ops' null checks (≤ 128 B `.text` on 3DS, ≤ 256 B on PC) and `.bss` by ≈ 260 B: AC-8's
   "same size" read as plan 018 read it (flagged, decision 30).
5. **The registrar is a static initializer** in a static library: linked only with `particle_batches.o`, which every
   particle drawing or destruction references. Probes (T8.3) check both ways (a particles game has the ops; `hello`
   has none) on PC and 3DS.
6. **Subnormals**: the velocity flush keeps velocities normal; positions, sizes and colours stay in normal ranges in
   the self-check; FTZ is documented as outside the domain otherwise (as spec 022).
7. **Compaction cost**: random lifetimes move most survivors every frame (stable order). Measured by `update1024`; a
   ring buffer for fixed lifetimes is a follow-up.
8. **Squares don't rotate** (a `FillRect`); textured particles do. Documented.
9. **Culling is conservative** (an L1 box around each quad): an off-view corner may still be "drawn" (clipped by the
   GPU). Documented; stats say so.
10. **`FrameTime` in data files is in ticks** (UX): documented with a table; a seconds/frames data kind is a follow-up.
11. **Merging with specs 023 and 025** (parallel worktrees): see "Merge notes"; the `gfx submit` profile scope is 023's.
12. **Bound emitters** update before the scene's update: their rate births follow the previous frame's transform
    (documented; unbind for the exact order).

## Merge notes (worktree from `main` after spec 022 is merged complete; 023 and 025 in parallel)

Spec 024 is built in its own worktree (branch `spec-024`) from `main` once spec 022's stage 11 is merged. Specs 023 and
025 are built in parallel worktrees and may merge before or after it. Every edit to a shared file is an insertion at
its own anchor; nothing is reordered or reformatted.

| Shared file | 022 (in the base) / 023 / 025 | 024's change, at a distinct anchor |
|-------------|-------------------------------|------------------------------------|
| `engine/core/CMakeLists.txt` | 023: `input_block.cpp` in the main list, a transition block after the camera block; 025: a pathfinding block after the `_nnh_query_sources` block | one `_nnh_particles_sources` block (incl. `src/gfx/particle_batches.cpp`, `-ffp-contract=off`) right after the `_nnh_hierarchy_sources` block's `endif()`, before `add_library(nnh::core ALIAS nnh_core)`; nothing in the main list |
| `tests/CMakeLists.txt` | 023: a block after the `camera_example` line, ctests after `camera_self_check_pc`; 025: a block before `add_test(NAME unit …)`, ctests after `data_self_check_pc` | **one** block (every 024 test, the platformer test with `level_effects.cpp`, the particles schema program and preset ctests, `target_sources(nnh_fuzz_targets …)`, an `if(NNH_BUILD_FUZZERS)` executable) right after the `entities_example/test_self_check.cpp` line; the two ctests right after the `entities_self_check_pc` block's `endif()`; nothing in the alphabetical list |
| `examples/CMakeLists.txt` | 023: `transitions` after `text`; 025: `pathfinding` between `input` and `platformer` | `add_subdirectory(particles)` right after `add_subdirectory(prefabs)` |
| `engine/core/src/gfx/gfx_frame.cpp` | 023: `NNH_PROFILE_SCOPE("gfx submit")` (and its include) at the top of `detail::gfx_submit()` | 024's hunks are in `free_all`, `submit_screen`'s loop, `gfx_init` and after `g_state.tiles.clear();`; if 023 isn't merged when the bench is built, copy 023's hunk **byte for byte** from the 023 branch (`git show spec-023:engine/core/src/gfx/gfx_frame.cpp`), so both branches carry the identical change; on a conflict keep 023's |
| `examples/platformer/level_play.{hpp,cpp}` | 022: barrels, the pass (base); 023: two one-line hunks (`enter_doors`' publish, `restart`'s spawn point) and a setter + member next to `in_door_` | header: the include after `#include "sparkles.hpp"`, `LevelEffects effects_;` after `sparkles_`; source: one statement after `camera_.apply();` at the end of `update()`, one after `turrets_.restart();`, one after `draw_sparkles();` — re-apply by hand on conflict |
| `examples/platformer/CMakeLists.txt`, `main.cpp` | 023: `door_transition.cpp`, `door_run.cpp` | `level_effects.cpp` right after `level_camera.cpp`; `.max_particles = 256` in `AppConfig::gfx` |
| `examples/platformer/tests/level.nnhs` | 022's regenerated lines (base); 023 and 025: never touched | **never touched**: the ctest and `--pc` script pass with the lines in the tree |
| `CLAUDE.md` | 023: after "Camera helpers" and the `queries` command; 025: after "Collision layers and queries" and the `ANIMATION_DEMO=check` line | the layout paragraph after the "Sprite animation" paragraph; the commands after the `nnh run data [--pc] …` line |
| `docs/api/gfx.md`, `docs/architecture/rendering.md` | 023 may add overlay notes | a "Particle batches" section after "Zoom (D33)" (rendering) and after the tile capacity section (gfx) |
| `docs/ROADMAP.md`, `docs/REFERENCES.md`, `decisions.md` change log, `specs/README.md` | their own lines and blocks | the "Particles" line only; 024's block appended; one change-log entry appended; the index row — on conflict keep both sides |
| `docs/architecture/decisions.md` table | 023/025 may edit their D38/D40 rows at stage 11 | D39's row sits between them (already on `main` with this plan): keep all three rows |
| `docs/architecture/patterns.md` | 022/023/025 may edit their rows | the "Batch draw command" row (already on `main`) between "Bridge" and "RAII handles" |
| `tests/framework/test_main.cpp` (`kMaxTests`) | maybe raised by 023/025 | raised only if T0.3 finds fewer than 400 left; the larger value wins |
| engine data module, `tools/assetc`, camera shake, `draw_list.*`, `draw2d.cpp`, `overlay.cpp`, `app.hpp`, `gfx.hpp`'s `Stats`, `app_3ds.cpp` | not touched by 023/025 (checked at merge with `git diff main...spec-023 --stat`) | as listed above |

After each merge the lead runs: the full ctest (pc-debug, pc-release), every example's `--pc` script, AC-2's golden,
022's, 023's and 025's goldens and the replay ctest with the tree's lines, and one 3ds-release build. No golden of 022,
023 or 025 may move because of 024 (024's gfx changes draw byte-identical items when no particle is drawn: stage 0
references), and 024's golden doesn't depend on theirs.

## Stage 10: console run (prepared)

T10.1 on the PC first: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh cmake --build --preset
3ds-dev --target particles_3dsx platformer_3dsx data_3dsx camera_3dsx`, without a warning; T7.6's table still
describes the objects; 3ds-release `hello.elf` within T4.6's budget. Then on an Old 3DS, nobody touching it after Y.
Five runs, each: open the Homebrew Launcher, press **Y**, then on the PC (`nnh` builds the Development `.3dsx`, sends
it, plays the script and brings the console back to the Homebrew Launcher; logs and screenshots in `build/logs/`):

1. `build/pc-debug/tools/nnh/nnh run particles --script examples/particles/tests/check.nnhs` (≈ 2 min). Expected, as on
   the PC (pc-debug), with the values recorded in "Stage 5 results" and "Stage 7 results": `particles: check started`,
   `particles: 600 sum <c1>` … `particles: 3600 sum <c6>`, **`particles: PASS <golden>`**, `particles: stats drawn <n>
   of 4096 dropped 0`; `particles: showcase`; after the touch at (120, 64) `burst: sparks 16 at 120,64 live 16` and
   later `done: sparks live 0 finished`; R → `effect: smoke`, a touch → `burst: smoke …`; A → `explosion: <n> live
   <n>`; X → `space: local`, X → `space: world`; B → `layer: front`; D-Pad Down → `smoke: stopped`; then `particles bench:
   started`, `update1024 … (target < 1.2 ms)`, `update16x64 … (target < 1.4 ms)`, `emit512 … (target < 1 ms)`, `draw
   particle P us sprite S us ratio R (target <= 0.75)`, `frame worst W ms missed M of 600`, `offscreen submit X ms culled
   512`, and the verdict `particles bench: PASS` or `particles bench: FAIL (<names>)`; `nnh: PASS (<n> lines, …)`.
   **Expected from the ARM pre-check** (T7.6 refines it): every logic line equal to the PC's; `update1024` 0.45–0.65 ms,
   `update16x64` 0.55–0.8, `frame` worst 9–12 ms with 0 missed, `offscreen` ≈ 0 — PASS; the draw ratio 0.52–0.70 —
   tight; `emit512` 1.05–1.35 ms — likely FAIL. A FAIL fails the run (`fail-on "particles bench: FAIL"`) after every
   number is logged: record them in "Console run 1"; T10.3 applies risk 1's list, or the budgets are set from the
   measurement with margin. Read the screenshots (gallery, check, touch burst, explosion, local, layer, bench): the
   same pictures as the `--pc` run's, rotated sprites within a pixel.
2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` (≈ 40 s). Expected:
   exactly the `replay: … sum` lines **in the tree** (spec 022's, unedited), `replay: done 900 frames sum <last>: 60 Hz
   PASS`; `nnh: PASS (79 lines, …)`. A different sum is a defect (the effects touch nothing hashed). Read the level
   screenshot: landing dust visible.
3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` (≈ 2.5 min). Expected:
   `bench: PASS 4 layers + 300 sprites, …` (the submit dispatch changed); `nnh: PASS`.
4. `build/pc-debug/tools/nnh/nnh run data --script examples/data/tests/check.nnhs` (≈ 20 s). Expected, unchanged since
   spec 014: `data check: PASS checksum 0xD5BD3020 (4 enemies, 3 items, 1 settings, index 595 bytes)` (the D29
   amendment changed no existing description); `nnh: PASS`.
5. `build/pc-debug/tools/nnh/nnh run camera --script examples/camera/tests/check.nnhs` (≈ 30 s). Expected, unchanged since
   spec 018: `camera: PASS 5201E6CF` (the shake's hash moved to `src/core/mix32.hpp`); `nnh: PASS`.

## Decisions taken (overnight, to review)

The spec's "Decisions to take" are approved as recommended. Its open questions and these plan-level decisions were
taken overnight, each the recommended or simplest option; the maintainer reviews them in the morning:

1. **Names**: module `particles`, header `<nnh/particles.hpp>`; `ParticleEmitter`, `ParticleDesc`, `ParticleRange`,
   `ParticleTimeRange`, `ParticleCount`, `ParticleBurst`, `ParticleColorKey`, `ParticleSizeKey`, `ParticleFrame`,
   `ParticleShape{Point, Box, Circle, Line}`, `ParticleDirection{Cone, Outward}`, `ParticleFrameMode{First, Random,
   OverLife, Rate}`, `ParticleSpace{World, Local}`, `ParticleOrder{OldestFirst, NewestFirst}`, `ParticleDrawOptions`,
   `ParticleStats`, `ParticleView`, `kMaxParticles` (8192) and the other limits; tag and log channel `particles`; example
   `examples/particles` (`PARTICLES_DEMO`, `PARTICLES_SHOT`).
2. **Open question "bytes per particle and layout"**: 48 B — a 32-byte hot record and a 16-byte cold one (hot/cold
   split, not one array per field); the console's `update1024` is the evidence.
3. **Appearance is evaluated, not stored**: colour, size and frame are functions of age computed when drawn or queried.
4. **Curves**: exact piecewise-linear segments (≤ 8) found through a 256-bucket table, not 256-entry value tables
   (which miss FR-12's bound on steep gradients); channels rounded; colours combined by an exact integer `mul8`.
5. **Open question "particle batches per frame"**: 32, in their own table; their commands use `tile_batch` indexes
   64..95, so `DrawList::build` is unchanged; a screen's split points grow to 96.
6. **Drawing is D39 option (b)**: expansion into `DrawItem2D` (Sprite, or FillRect without a texture) through a
   128-item scratch, `draw_2d` per chunk; no contract change. The contract fallback stays risk 1's last step.
7. **gfx reaches the particle code through an ops pointer** set by a static registrar in `gfx/particle_batches.o`.
8. **Placement is shared** with `DrawList::build` (`gfx/sprite_place.hpp`), so items equal `gfx::draw`'s by
   construction.
9. **One IEEE division per particle at birth** (`1/lifetime`): correctly rounded like `×`, so FR-16 holds; no division
   in a per-frame loop.
10. **Damping is linear per frame** (`keep = 1 − damping/60`, damping 0..60 per second), not `(1 − d)^(1/60)` (no `pow`).
11. **Velocity flush** below 2⁻⁶⁰ (integer exponent test) for PC/3DS agreement under flush-to-zero.
12. **Area-uniform discs and rings without `sqrt`** (`max(u₁, u₂)` and a fixed mixture); outward directions of box and
    line points by a fixed Newton reciprocal square root; a point (or the centre) falls back to the cone.
13. **Sizes**: a textured particle's size scales its frame; without a texture it is the square's side in pixels;
    world-space sizes ignore the emitter's scale, local-space ones multiply it; velocities and shapes go through the map
    (a scaled emitter is faster and bigger).
14. **Open question "preset extension and schema source"**: extension `.particles`, source
    `engine/core/schemas/particles_schema.cpp`, CMake variable `NNH_PARTICLES_SCHEMA`; `FrameTime` written in ticks in
    files (a seconds/frames data kind is a follow-up).
15. **D29 amended** with two opt-in field rules, `.ascending` and `.up_to`, written into a description only when used
    (no existing fingerprint changes); the description marks unused keys (`at < 0`) and frames (`region.w < 0`).
16. **Sub-frame spreading** interpolates the origin and the drawing values (one sine only when the rotation changed);
    `f = 1` is the current map exactly; local space is not spread; bursts are not spread.
17. **`set_transform(…, jump)`** cancels spreading and inherited velocity across a teleport (a small addition the spec
    doesn't name; Godot users ask for it).
18. **`emit_at(point)`** takes the point in the emitter's space and keeps its axes; `emit_at(transform)` takes another
    orientation.
19. **Bursts** need an interval ≥ 1 frame when they repeat (at most one firing per burst per frame); counts are hashed
    from (seed, burst, cycle).
20. **The full case**: drop and count; one Debug/Development warning per emitter the first time (none in Release).
21. **`finished()`** = emission over (not started, a duration ended without looping, or no rate and every burst done)
    and no live particle.
22. **Culling** is conservative (an L1 box per quad, the emitter's bounds grown by its largest quad).
23. **Squares don't rotate**; drawing an emitter always records a command (one of `max_draws`), even when empty.
24. **Scene binding**: `bind(scene)`/`unbind()`, D30 unchanged (`BeforeUpdate` → update, `AfterExit` → clear).
25. **`check(desc)`** is public (a preset loaded at run time can be checked before `set_desc`).
26. **Open questions "random generator", "collision", "additive blending", "replace oldest", "align to velocity"**:
    follow-ups, as the spec recommends.
27. **Example layout and controls** as described above (Old 3DS buttons only), with a code-built tile band for the
    layer demonstration and two presets.
28. **Platformer**: dust, puff and flash as texture-less squares in `level_effects.*`, reading state only; three
    one-line hooks in `level_play.cpp`; the replay lines untouched.
29. **Bench**: costs from the scene's scope plus plan 023's `gfx submit` scope minus a base run; "the sprites bench
    still passes" is covered by `gate.nnhs` and by the bench's `sprite512` line (a regression shows as a sprite cost above
    ≈ 13 µs).
30. **`hello`**: AC-8's "same size" is read as plan 018 read it — no particle symbol, `.text` ≤ +128 B (3DS) / +256 B
    (PC) for the null checks and the split dispatch, `.bss` ≈ +260 B; measured at T4.6.
31. **AC-7 targets kept as written**; `emit512` flagged likely over, the draw ratio tight.
32. **A fuzz target** `fuzz_particle_ops` (mini-fuzz in `nnh_tests`, libFuzzer with `NNH_BUILD_FUZZERS`).
33. **Console stage**: five scripted runs (particles `check.nnhs` with the bench; platformer `level.nnhs` and
    `gate.nnhs`; data `check.nnhs` for D29; camera `check.nnhs` for the moved hash).

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Portable core; integer generator; spec 022's sine; a fixed update order; integer sign, finiteness and key tests; the velocity flush; AC-2 on every compiler and the console; rasterisation of rotated texels excepted |
| II | ✅ | 48 B per particle, ≈ 2.1 KB per emitter, gfx storage only when enabled; one cache line per particle per update; L1-resident expansion; no `vmrs`, `__aeabi_*`, `memcpy` in hot loops; the disassembly checked before the console; one target likely over and one tight, with AC-7's fallback |
| III | ✅ | One light header; measured; presets are data-only rebuilds |
| IV | ✅ | No events |
| V | ✅ | No allocation after creation, no `virtual`, RTTI or exceptions; no game code called |
| VI | ✅ | Tests first; references and statistical bounds; the expansion against `DrawList::build`; golden; fuzz; mutation pass |
| VII | ✅ | One module of 14 small sources; gfx gains one pair and one inline header |
| VIII | ✅ | API page, linked pages, example, platformer, D39 |
| IX | ✅ | Own code, prior art cited, original art |
| X | ✅ | Presets through spec 014's loader with two new checked rules; every description re-checked; misuse loud in Debug, harmless in Release; fuzzed |
| XI | ✅ | Optional module; gfx reaches it through a null-unless-linked pointer; `max_particles` 0 by default; the schema opt-in; `hello` within a stated budget (decision 30) |

No constitution exception is requested. D39 is new (see [decisions.md](../../docs/architecture/decisions.md)); D26 is
generalised, not changed (its tile batches keep their table, indexes and quads; a second batch kind shares the split
points); D29 is amended (two opt-in field rules, an engine-shipped opt-in schema source); D30 and D33 are not amended
(emitters are one more attachment user; particles zoom through the sprite placement); D37 is used as is. The patterns
page gains a "Batch draw command" row. Flagged for the maintainer: AC-8's `hello` budget (decision 30), `emit512`
(risk 1), the D29 amendment (decision 15).

## Stage results and refinements

### Stage 0 results

Windows, WinLibs GCC (pc presets) and devkitARM (3DS presets), 2026-11-24, worktree `spec-024` at `18297dc` (spec 022
complete and spec 025 merged). Scripts in `build/ref-024/` (not committed; adapted from `build/ref-022/`). Other
worktrees (specs 023, 026, 027) were building on the same PC at the same time: every time below is noisy.

- **`hello` sizes (bytes)**: pc-release `hello.exe` 278 856 (`text` 130 668, `data` 22 020, `bss` 211 696); pc-debug
  1 297 082 (`text` 198 688); 3ds-release `hello.elf` 2 488 800 (`text` 201 124, `data` 28 352, `bss` 106 680),
  `.3dsx` 246 512; 3ds-dev `hello.elf` 2 748 028 (`text` 217 896), `.3dsx` 268 012. `nm -C` lists saved
  (`hello.pc.ref.nm`, `hello.3ds.ref.nm`).
- **`.text` (3ds-release)**: sprites 205 980, platformer 345 812, data 283 564, camera 222 164.
- **Build times** (3 runs, centiseconds): edit `examples/prefabs/room.cpp` → pc-debug `prefabs` 224 / 230 / 244,
  3ds-release `prefabs_3dsx` 527 / 532 / 555; a data-only rebuild of `examples/data` (`goblin.enemy` touched) 91 / 85
  / 116, no C++ compiled.
- **Header cost**: a one-line TU including `<nnh/gfx.hpp>`, `<nnh/transform.hpp>`, `<nnh/time.hpp>`,
  `<nnh/result.hpp>`: 8 037 preprocessed lines, `-fsyntax-only` 0.46–0.57 s (5 runs).
- **3ds-dev disassembly** (`dis0/`): `draw_list.o`, `gfx_frame.o`, `draw2d.o`, `tile_batches.o`, `overlay.o`,
  `camera_shake.o`, `draw2d_3ds.o`.
- **Goldens** (their ctests): replay `platformer_replay_matches_script` with the tree's lines (spec 022's `E445E8B2`
  set), `camera: PASS 5201E6CF`, `queries: PASS 4ACAA177`, `entities: PASS C8FB9A41`, `prefabs: PASS 7BB7901E`,
  `hierarchy: PASS 23DB5C36`, `tweens: PASS B44CA3F6`, `pathfinding: PASS 1C0B5992`, `data check: PASS checksum
  0xD5BD3020`. ctest pc-debug 273/273, pc-release 259/259; `nnh_tests` 2569 (pc-release 2529), `kMaxTests` 4096: 1527
  left, no raise needed.
- **Reference screenshots** (`ref/`, taken twice: identical): hello, sprites, sprites bench, scenes, camera zoom1,
  queries all, entities, prefabs, hierarchy, pathfinding, platformer bench (top), level 1 at 90, replay at 64 and 312.
