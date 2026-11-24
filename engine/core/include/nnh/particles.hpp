#pragma once
/// @file nnh/particles.hpp
/// Particles (spec 024, decision D39): emitters the game creates and owns, each one block of tagged memory with a fixed
/// capacity, described by one plain struct (ParticleDesc, data-loadable), updated once per 60 Hz frame and drawn with
/// one draw-list command per draw (expanded at submit within AppConfig::gfx.max_particles). The simulation is the same
/// bits on PC and 3DS: seeded counter-based randomness, the engine's sine, no libm. Optional: a game that doesn't include
/// this header links nothing of it. See docs/api/particles.md.
///
/// Light: <nnh/math.hpp>, <nnh/time.hpp>, <nnh/gfx.hpp>, <nnh/transform.hpp>, <nnh/result.hpp>; declarations only (every
/// float operation lives in src/particles/*.cpp, compiled with -ffp-contract=off).
///
/// @code
/// nnh::ParticleDesc sparks{.spread = nnh::kFullTurn, .speed = {60, 120}, .gravity = {0, 200},
///                          .lifetime = {nnh::Frames{20}, nnh::Frames{30}}, .color_min = nnh::colors::yellow,
///                          .color_max = nnh::colors::yellow, .size = {2, 3}};
/// auto fx = nnh::ParticleEmitter::create(sparks, 256);      // in on_enter (allocates once)
/// fx.value().emit_at(coin_position, 16);                     // when a coin is taken
/// fx.value().update();                                       // once per frame
/// fx.value().draw(nnh::Screen::Top, {.layer = 3});           // squares without a texture
/// @endcode

#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/math.hpp"
#include "nnh/result.hpp"
#include "nnh/texture.hpp" // complete, not declared: GCC 13 crashes on draw(s, {.layer = n}) against an incomplete one
#include "nnh/time.hpp"
#include "nnh/transform.hpp"

namespace nnh {

struct MemTag; // <nnh/memory.hpp>
class Scene;   // <nnh/scene.hpp>

namespace detail {
struct ParticleBlock;
}

inline constexpr int kMaxParticles = 8192;      ///< capacity of one emitter
inline constexpr int kMaxParticleBursts = 8;    ///< bursts per description
inline constexpr int kMaxParticleKeys = 8;      ///< colour keys; size keys
inline constexpr int kMaxParticleFrames = 16;   ///< sprite frames per description
inline constexpr int kMaxParticlePrewarm = 600; ///< prewarm frames

/// Where particles are born, in the emitter's local space (FR-8).
enum class ParticleShape : std::uint8_t { Point, Box, Circle, Line };
/// How a particle's initial direction is chosen (FR-9).
enum class ParticleDirection : std::uint8_t { Cone, Outward };
/// Which frame a particle shows (FR-13).
enum class ParticleFrameMode : std::uint8_t { First, Random, OverLife, Rate };
/// World: born at the emitter, then on their own (a trail). Local: in the emitter's frame (a flame on a torch).
enum class ParticleSpace : std::uint8_t { World, Local };
/// Drawing order within one emitter: the oldest first (newest on top) or the newest first.
enum class ParticleOrder : std::uint8_t { OldestFirst, NewestFirst };

/// A uniform range: each particle draws a value between `min` and `max` (equal: no randomness).
struct ParticleRange {
    float min = 0;
    float max = 0;
};
/// A range of frame-clock times (lifetimes).
struct ParticleTimeRange {
    FrameTime min = Frames{60};
    FrameTime max = Frames{60};
};
/// A range of counts (bursts).
struct ParticleCount {
    std::uint16_t min = 0;
    std::uint16_t max = 0;
};
/// A burst (FR-5): fires at `at` after the start (and the delay), `cycles` times (0 = for ever) every `interval` (at
/// least one frame when it repeats), `count` particles each time (hashed from the seed, burst and cycle). A burst with
/// `count.max == 0` is unused (the default).
struct ParticleBurst {
    FrameTime at{};
    ParticleCount count{};
    std::uint16_t cycles = 1;
    FrameTime interval = Frames{1};
};
/// A colour key: `at` 0..1 of a particle's life; < 0: unused (the default). Used keys come first, ascending.
struct ParticleColorKey {
    float at = -1;
    Color color = colors::white;
};
/// A size key: `size` multiplies the start size; `at` as for colour keys.
struct ParticleSizeKey {
    float at = -1;
    float size = 1;
};
/// A region of the texture given to draw() and its pivot. `region.w < 0`: unused (the default); an empty region (w or h
/// 0): the whole texture. `center_pivot`: the region's centre, else `pivot` in region pixels.
struct ParticleFrame {
    Region region{0, 0, -1, -1};
    Vec2 pivot{};
    bool center_pivot = true;
};

/// Every value of an emitter (FR-2): plain, trivially copyable, data-loadable (engine/core/schemas). The default is a
/// valid emitter: white 1-pixel particles at a point, 60 frames of life, no motion, no emission until asked (emit()).
/// Units: pixels, pixels per second (and per second²), radians clockwise on the y-down screen, FrameTime for times.
struct ParticleDesc {
    // Emission (FR-4, FR-5)
    float rate = 0;       ///< particles per second, 0..491 520 (8192 per frame)
    FrameTime delay{};    ///< before the first emission (once per start)
    FrameTime duration{}; ///< of the emission; 0: no end
    bool looping = false; ///< at the end of `duration`, start again (bursts re-armed)
    // The arrays have no `= {}`: their elements' own member defaults apply either way, and with one GCC 13 and 14
    // refuse a braced `{}` / `{.rate = ...}` ParticleDesc argument (as PrefabOptions::children, spec 022 stage 9).
    ParticleBurst bursts[kMaxParticleBursts];
    // Shape, in local space (FR-8)
    ParticleShape shape = ParticleShape::Point;
    Vec2 box{};              ///< Box: size, centred on the origin
    bool edge = false;       ///< Box: on the outline only
    float radius = 0;        ///< Circle: outer radius
    float inner_radius = 0;  ///< Circle: a ring from here to `radius` (equal: on the circle)
    float arc_start = 0;     ///< Circle: radians clockwise from +x
    float arc = kFullTurn;   ///< Circle: the arc's width, 0..kFullTurn
    float length = 0;        ///< Line: along local x, centred
    // Direction and speed (FR-9)
    ParticleDirection direction = ParticleDirection::Cone;
    float angle = 0;            ///< Cone: radians clockwise from +x (−kQuarterTurn = up)
    float spread = 0;           ///< Cone: full width, 0..kFullTurn (kFullTurn: every direction)
    ParticleRange speed{};      ///< px/s; may be negative
    float inherit_velocity = 0; ///< 0..1 of the emitter's own velocity (world space)
    // Motion (FR-11)
    Vec2 gravity{};             ///< px/s², local space (mapped through the transform)
    bool world_gravity = false; ///< gravity in world space even for a turned or local-space emitter
    float damping = 0;          ///< 1/s, 0..60: each frame keeps 1 − damping/60 of the velocity
    ParticleRange rotation{};   ///< initial, radians
    ParticleRange spin{};       ///< rad/s
    ParticleTimeRange lifetime{};
    // Appearance (FR-12, FR-13)
    Color color_min = colors::white; ///< start colour: one random point between the two (every channel)
    Color color_max = colors::white;
    ParticleRange size{1, 1}; ///< start size ≥ 0: a scale of the frame, or the side in pixels without a texture
    ParticleColorKey color_keys[kMaxParticleKeys];
    ParticleSizeKey size_keys[kMaxParticleKeys];
    ParticleFrame frames[kMaxParticleFrames];
    ParticleFrameMode frame_mode = ParticleFrameMode::First;
    std::uint8_t frame_repeats = 1; ///< OverLife: times through the frames, 1..255
    float frame_rate = 0;           ///< Rate: frames per second, 0..600
    // Space, order, determinism (FR-14, FR-18, FR-15, FR-7)
    ParticleSpace space = ParticleSpace::World;
    ParticleOrder order = ParticleOrder::OldestFirst;
    std::uint32_t seed = 1;
    std::uint16_t prewarm = 0; ///< frames simulated at creation and restart (≤ 600; cost: frames × alive)
};

/// How draw() places an emitter's particles (FR-19).
struct ParticleDrawOptions {
    int layer = 0;              ///< 0..15, sorted with sprites and tile layers by layer, then call (D26)
    bool ignore_camera = false; ///< screen pixels (HUD effects)
    Color tint = colors::white; ///< multiplied into every particle's colour
    float opacity = 1;          ///< 0..1, multiplies the tint's alpha
};

/// An emitter's counters (FR-22).
struct ParticleStats {
    int live = 0;
    int peak = 0;
    int capacity = 0;
    std::uint32_t emitted = 0; ///< particles created since creation or the last restart()
    std::uint32_t dropped = 0; ///< asked for while full (since creation or the last restart())
    int drawn = 0;             ///< by the last frame that drew it (both screens, every draw)
    int culled = 0;            ///< skipped by that frame's culling
};

/// One live particle as drawn (FR-22), in the emitter's space (world, or local for a local-space emitter).
struct ParticleView {
    Vec2 position{};
    Vec2 velocity{}; ///< px per frame (× 60 = px/s)
    float rotation = 0;
    float size = 0;  ///< start size × the size curve
    Color color{};   ///< start colour × the gradient
    int frame = 0;
    FrameTime age{};
    FrameTime lifetime{};
};

/// An emitter (FR-1): a handle to one block the game owns (a scene member, a level member). Movable, not copyable.
/// Nothing allocates after create(); update(), emit() and draw() call no game code and log nothing in the normal path.
/// Misuse (an empty emitter, non-finite values, a negative count) asserts in Debug/Development and does nothing in
/// Release.
class ParticleEmitter {
public:
    /// One block of `tag` (default "particles"): 48 bytes per particle plus about 2.3 KB. Errors: InvalidArgument (a
    /// description out of range, named; `capacity` outside 1..kMaxParticles), OutOfMemory. Starts emitting and runs
    /// `desc.prewarm` frames. Load-time cost.
    static Result<ParticleEmitter> create(const ParticleDesc& desc, int capacity, MemTag* tag = nullptr);
    /// The checks of create() without creating (a preset loaded at run time). The message names the first bad field.
    static Result<void> check(const ParticleDesc& desc);

    ParticleEmitter() = default; ///< empty: commands assert and do nothing, queries answer zero
    ~ParticleEmitter();          ///< unbinds, forgets this frame's draws, frees the block
    ParticleEmitter(ParticleEmitter&& other) noexcept;
    ParticleEmitter& operator=(ParticleEmitter&& other) noexcept;
    ParticleEmitter(const ParticleEmitter&) = delete;
    ParticleEmitter& operator=(const ParticleEmitter&) = delete;
    bool valid() const;

    const ParticleDesc& desc() const; ///< a default description for an empty emitter
    /// Replaces the description (tables rebuilt: load-time cost). Live particles keep their drawn values; a bad
    /// description asserts and changes nothing. The capacity can't change.
    void set_desc(const ParticleDesc& desc);
    /// The seed of later emissions and restarts (the description's `seed` is overridden).
    void set_seed(std::uint32_t seed);

    /// Where the emitter is (FR-14). `jump`: no sub-frame spreading or inherited velocity across this move (a
    /// teleport). Non-finite values assert and are ignored.
    void set_transform(const Transform2D& t, bool jump = false);
    void set_transform(const WorldTransform2D& t, bool jump = false); ///< a node's world (spec 022)
    void set_position(Vec2 position, bool jump = false);              ///< rotation and scale kept
    const WorldTransform2D& transform() const;

    void start();   ///< emits again (an ended or stopped emission restarts its clock, delay and bursts); no-op if emitting
    void stop();    ///< no more rate or burst emission; live particles finish
    void clear();   ///< removes every particle now (clock and random sequence unchanged)
    void restart(); ///< clear + clock, bursts and random sequence from the start + start + prewarm: the same bits again
    bool emitting() const; ///< started and not ended
    bool finished() const; ///< no rate or burst emission left (stopped, ended, or a finished one-shot) and none alive
    int alive() const;

    /// On demand (FR-6), even when stopped, at age 0. @return the particles created (the rest dropped and counted).
    int emit(int count);
    /// At `point` in the emitter's space (world, or local for a local-space emitter), its axes kept.
    int emit_at(Vec2 point, int count);
    /// At another place and orientation (world space; local space: a transform in the emitter's frame).
    int emit_at(const WorldTransform2D& at, int count);

    /// One 60 Hz frame (FR-17): the clock, motion and deaths, rate births spread along the emitter's movement, bursts.
    /// No allocation, no game code, no log.
    void update();
    /// D30: the scene's BeforeUpdate runs update(), AfterExit clears. Bind once; keep the emitter alive while bound.
    void bind(Scene& scene);
    void unbind();

    /// One draw-list command (FR-19), read at the end of the frame (the emitter's state then). Without a texture: solid
    /// squares of `size` pixels (not rotated). Needs AppConfig::gfx.max_particles > 0. Only inside a frame.
    void draw(Screen screen, const ParticleDrawOptions& options = {}) const;
    void draw(Screen screen, const Texture& texture, const ParticleDrawOptions& options = {}) const;

    int capacity() const;
    /// The live particles' positions after the last update or emission, in world space (empty: zero size at the
    /// emitter's origin).
    Rect bounds() const;
    /// Particle `index` (0 = the oldest); asserts outside 0..alive() − 1 (a zero view).
    ParticleView particle(int index) const;
    ParticleStats stats() const;

private:
    friend struct ParticleAccess;
    detail::ParticleBlock* block_ = nullptr;
};

} // namespace nnh
