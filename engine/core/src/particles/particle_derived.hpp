#pragma once
// Values derived from a description at create() and set_desc() (spec 024 plan "Derived at creation"): per-frame units,
// the curves' segments and bucket tables, the frame rule's constants, the shape sampler and the variant flags. Nothing
// here is computed per frame.

#include <cstdint>

#include "nnh/particles.hpp"

namespace nnh::detail {

inline constexpr int kCurveSegments = kMaxParticleKeys + 1; // before the first key, between keys, after the last
inline constexpr std::uint32_t kRateThreshold = 60u * 65536u; // one particle per crossing of the 16.16 accumulator

/// A piecewise-linear curve of `channels` values over t in [0, 1): segment s covers [start[s], start[s + 1]).
template <int Channels>
struct ParticleCurve {
    float start[kCurveSegments + 1] = {}; ///< start[count] = 2: a sentinel above every t
    float inv_len[kCurveSegments] = {};   ///< 1 / length, 0 for a constant or zero-length segment
    float v0[kCurveSegments][Channels] = {};
    float dv[kCurveSegments][Channels] = {};
    std::uint8_t bucket[256] = {}; ///< the segment holding t = b / 256 (the last one starting at or before it)
    int count = 1;
};
using ParticleColorCurve = ParticleCurve<4>;
using ParticleSizeCurve = ParticleCurve<1>;

struct ParticleDerived;

/// A shape sample in local space: a position and, when the shape gives one, an outward unit direction.
struct ShapeSample {
    Vec2 position{};
    Vec2 outward{};
    bool has_outward = false;
};
using ShapeSampler = void (*)(const ParticleDerived& d, std::uint32_t base, ShapeSample& out);

struct ParticleDerived {
    // Emission
    /// The rate in 16.16 particles per second = rate_whole × kRateThreshold + rate_frac per frame: each frame makes
    /// rate_whole particles plus one more each time the accumulator crosses the threshold (exact for every rate).
    std::uint32_t rate_whole = 0;
    std::uint32_t rate_frac = 0;
    bool rate = false;
    float rate_first = 0;   ///< frame fraction per unit of (threshold − accumulator): 1 / step
    float rate_spacing = 0; ///< frame fraction between two crossings: threshold / step
    // Shape
    ShapeSampler sample = nullptr;
    Vec2 box{};
    Vec2 half_box{};
    float edge_top = 0, edge_right = 0, edge_bottom = 0, perimeter = 0; ///< Box edge: cumulative lengths
    float inner = 0, ring_span = 0, ring_uniform = 0; ///< Circle: radius = inner + span × x
    float arc_start = 0, arc = 0;
    float length = 0;
    // Direction and speed, per frame
    bool cone = true;
    float cone_start = 0, cone_spread = 0;
    float speed_min = 0, speed_span = 0;
    float inherit = 0;
    // Motion, per frame
    Vec2 gravity{};
    float keep = 1;
    bool drag = false;
    bool spin = false;
    float rotation_min = 0, rotation_span = 0;
    float spin_min = 0, spin_span = 0;
    std::uint32_t life_min = 0, life_count = 1; ///< ticks: min + hash_below(h, count)
    // Appearance
    std::uint8_t color_min[4] = {};
    float color_span[4] = {};
    float size_min = 0, size_span = 0;
    float largest_size = 0; ///< the largest start size × the largest size key (culling)
    ParticleColorCurve color{};
    ParticleSizeCurve size{};
    int frames = 1;              ///< n: frames used (1 without any: the whole texture)
    bool random_frame = false;   ///< the cold frame byte is the base
    float frame_per_tick = 0;    ///< Rate: frames per tick
    float frame_per_life = 0;    ///< OverLife: n × repeats
    std::uint32_t frame_last = 0x00FFFFFFu; ///< m is clamped to this (OverLife: n × repeats − 1)
    std::uint32_t frame_recip = 0; ///< ceil(2²⁸ / n): m mod n without a division (m < 2²⁴)
    bool world = true;
    bool newest_first = false;
};

/// Fills `out` from a checked description.
void particle_derive(const ParticleDesc& desc, ParticleDerived& out);

/// Builds a curve from used keys (`at` ≥ 0, ascending): none → `fallback`, one → constant.
void build_color_curve(const ParticleColorKey* keys, int count, ParticleColorCurve& out);
void build_size_curve(const ParticleSizeKey* keys, int count, ParticleSizeCurve& out);

/// The number of used entries (the first with `at` < 0, or region.w < 0, ends the list).
int used_color_keys(const ParticleDesc& d);
int used_size_keys(const ParticleDesc& d);
int used_frames(const ParticleDesc& d);

} // namespace nnh::detail
