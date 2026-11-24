// Values derived from a checked description (spec 024 plan "Derived at creation"): per-frame units (× 1/60, × 1/3600),
// the rate's 16.16 step, the curves (segments + 256-bucket tables), the frame rule's constants, the shape sampler and
// the variant flags. Load-time only; every division here is by a value fixed for the emitter.

#include "particles/particle_derived.hpp"

#include "particles/particle_bits.hpp"
#include "particles/particle_shapes.hpp"

namespace nnh::detail {
namespace {

/// The segments' bucket table: for t = b / 256, the last segment starting at or before it.
template <int Channels>
void fill_buckets(ParticleCurve<Channels>& c) {
    int s = 0;
    for (int b = 0; b < 256; ++b) {
        const float t = static_cast<float>(b) * (1.0f / 256.0f);
        while (s + 1 < c.count && c.start[s + 1] <= t) ++s;
        c.bucket[b] = static_cast<std::uint8_t>(s);
    }
}

/// Segments from key positions `at[0..n)` and values `value(i)`: constant before the first key, linear between keys
/// (a zero-length segment for equal positions), constant after the last.
template <int Channels, typename Value>
void build_curve(const float* at, int n, Value value, ParticleCurve<Channels>& c) {
    c = ParticleCurve<Channels>{};
    float v[Channels];
    if (n <= 1) {
        value(0, v); // n == 0: the fallback
        for (int k = 0; k < Channels; ++k) c.v0[0][k] = v[k];
        c.count = 1;
    } else {
        value(0, v);
        for (int k = 0; k < Channels; ++k) c.v0[0][k] = v[k]; // [0, at0): the first key's value
        c.start[0] = 0;
        for (int i = 0; i + 1 < n; ++i) {
            float a[Channels], b[Channels];
            value(i, a);
            value(i + 1, b);
            const int s = i + 1;
            c.start[s] = at[i];
            const float len = at[i + 1] - at[i];
            c.inv_len[s] = len > 0 ? 1.0f / len : 0.0f;
            for (int k = 0; k < Channels; ++k) {
                c.v0[s][k] = a[k];
                c.dv[s][k] = c.inv_len[s] > 0 ? b[k] - a[k] : 0.0f;
            }
        }
        value(n - 1, v);
        c.start[n] = at[n - 1];
        for (int k = 0; k < Channels; ++k) c.v0[n][k] = v[k];
        c.count = n + 1;
    }
    c.start[c.count] = 2.0f; // above every t
    fill_buckets(c);
}

void derive_emission(const ParticleDesc& d, ParticleDerived& out) {
    // rate ≤ 491 520 = 2^15 · 15: × 65536 fits 35 bits, exact in double.
    const auto step = static_cast<std::uint64_t>(static_cast<double>(d.rate) * 65536.0 + 0.5);
    out.rate_whole = static_cast<std::uint32_t>(step / kRateThreshold);
    out.rate_frac = static_cast<std::uint32_t>(step % kRateThreshold);
    out.rate = step > 0;
    if (out.rate) {
        out.rate_first = static_cast<float>(1.0 / static_cast<double>(step));
        out.rate_spacing = static_cast<float>(static_cast<double>(kRateThreshold) / static_cast<double>(step));
    }
}

void derive_shape(const ParticleDesc& d, ParticleDerived& out) {
    out.box = d.box;
    out.half_box = {d.box.x * 0.5f, d.box.y * 0.5f};
    out.edge_top = d.box.x;
    out.edge_right = d.box.x + d.box.y;
    out.edge_bottom = d.box.x + d.box.y + d.box.x;
    out.perimeter = out.edge_bottom + d.box.y;
    out.inner = d.inner_radius;
    out.ring_span = d.radius - d.inner_radius;
    const float sum = d.inner_radius + d.radius;
    out.ring_uniform = sum > 0 ? (2.0f * d.inner_radius) / sum : 0.0f;
    out.ring_pick = fraction_threshold(out.ring_uniform);
    out.arc_start = d.arc_start;
    out.arc = d.arc;
    out.length = d.length;
    out.births = births_for(d.shape, d.edge);
    out.cone = d.direction == ParticleDirection::Cone;
    out.cone_start = d.angle - d.spread * 0.5f;
    out.cone_spread = d.spread;
}

void derive_motion(const ParticleDesc& d, ParticleDerived& out) {
    const float speed_min = flush_tiny(d.speed.min / 60.0f);
    const float speed_max = flush_tiny(d.speed.max / 60.0f);
    out.speed_min = speed_min;
    out.speed_span = flush_tiny(speed_max - speed_min);
    out.inherit = d.inherit_velocity;
    out.gravity = {flush_tiny(d.gravity.x / 3600.0f), flush_tiny(d.gravity.y / 3600.0f)};
    out.keep = 1.0f - d.damping / 60.0f;
    out.drag = d.damping > 0;
    out.rotation_min = d.rotation.min;
    out.rotation_span = d.rotation.max - d.rotation.min;
    const float spin_min = flush_tiny(d.spin.min / 60.0f);
    const float spin_max = flush_tiny(d.spin.max / 60.0f);
    out.spin_min = spin_min;
    out.spin_span = flush_tiny(spin_max - spin_min);
    out.spin = spin_min != 0 || spin_max != 0;
    out.life_min = static_cast<std::uint32_t>(d.lifetime.min.ticks);
    out.life_count = static_cast<std::uint32_t>(d.lifetime.max.ticks - d.lifetime.min.ticks) + 1u;
}

void derive_appearance(const ParticleDesc& d, ParticleDerived& out) {
    const std::uint8_t lo[4] = {d.color_min.r, d.color_min.g, d.color_min.b, d.color_min.a};
    const std::uint8_t hi[4] = {d.color_max.r, d.color_max.g, d.color_max.b, d.color_max.a};
    for (int k = 0; k < 4; ++k) {
        out.color_base[k] = static_cast<float>(lo[k]);
        out.color_span[k] = static_cast<float>(hi[k]) - static_cast<float>(lo[k]);
    }
    out.size_min = d.size.min;
    out.size_span = d.size.max - d.size.min;
    build_color_curve(d.color_keys, used_color_keys(d), out.color);
    build_size_curve(d.size_keys, used_size_keys(d), out.size);
    float largest_key = used_size_keys(d) == 0 ? 1.0f : 0.0f;
    for (int i = 0; i < used_size_keys(d); ++i) largest_key = d.size_keys[i].size > largest_key ? d.size_keys[i].size : largest_key;
    const float largest_start = d.size.max > d.size.min ? d.size.max : d.size.min;
    out.largest_size = largest_start * largest_key;
    const int n = used_frames(d) > 0 ? used_frames(d) : 1;
    out.frames = n;
    out.random_frame = d.frame_mode == ParticleFrameMode::Random;
    out.frame_per_tick = d.frame_mode == ParticleFrameMode::Rate ? d.frame_rate / 245760.0f : 0.0f;
    out.frame_per_life = d.frame_mode == ParticleFrameMode::OverLife ? static_cast<float>(n * d.frame_repeats) : 0.0f;
    out.frame_last = d.frame_mode == ParticleFrameMode::OverLife ? static_cast<std::uint32_t>(n * d.frame_repeats - 1)
                                                                  : 0x00FFFFFFu;
    out.frame_recip = ((1u << 28) + static_cast<std::uint32_t>(n) - 1u) / static_cast<std::uint32_t>(n);
    out.world = d.space == ParticleSpace::World;
    out.newest_first = d.order == ParticleOrder::NewestFirst;
}

} // namespace

std::uint32_t fraction_threshold(float fraction) {
    const float x = fraction * 16777216.0f; // exact: a power of two
    if (!(x > 0.0f)) return 0;
    if (x >= 16777216.0f) return 16777216u;
    auto t = static_cast<std::uint32_t>(x); // floor
    if (static_cast<float>(t) < x) ++t;     // ceil: m < x ⇔ m < ⌈x⌉ for a whole m
    return t;
}

int used_color_keys(const ParticleDesc& d) {
    int n = 0;
    while (n < kMaxParticleKeys && d.color_keys[n].at >= 0) ++n;
    return n;
}

int used_size_keys(const ParticleDesc& d) {
    int n = 0;
    while (n < kMaxParticleKeys && d.size_keys[n].at >= 0) ++n;
    return n;
}

int used_frames(const ParticleDesc& d) {
    int n = 0;
    while (n < kMaxParticleFrames && d.frames[n].region.w >= 0) ++n;
    return n;
}

void build_color_curve(const ParticleColorKey* keys, int count, ParticleColorCurve& out) {
    float at[kMaxParticleKeys] = {};
    for (int i = 0; i < count; ++i) at[i] = keys[i].at;
    build_curve<4>(at, count,
                   [&](int i, float* v) {
                       const Color c = count == 0 ? colors::white : keys[i].color;
                       v[0] = c.r;
                       v[1] = c.g;
                       v[2] = c.b;
                       v[3] = c.a;
                   },
                   out);
}

void build_size_curve(const ParticleSizeKey* keys, int count, ParticleSizeCurve& out) {
    float at[kMaxParticleKeys] = {};
    for (int i = 0; i < count; ++i) at[i] = keys[i].at;
    build_curve<1>(at, count, [&](int i, float* v) { v[0] = count == 0 ? 1.0f : keys[i].size; }, out);
}

void particle_derive(const ParticleDesc& desc, ParticleDerived& out) {
    out = ParticleDerived{};
    derive_emission(desc, out);
    derive_shape(desc, out);
    derive_motion(desc, out);
    derive_appearance(desc, out);
}

} // namespace nnh::detail
