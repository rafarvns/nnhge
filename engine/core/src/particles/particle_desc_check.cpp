// The description checks (spec 024 FR-2, FR-25, plan "Description checks"): every float finite, every value in its
// range, keys and frames used first and keys ascending. The first bad field is named.

#include "particles/particle_desc_check.hpp"

#include <cstring>

#include "particles/particle_bits.hpp"

namespace nnh::detail {
namespace {

/// Records the first failure only.
struct Checker {
    Error error{ErrorCode::None, {}};
    bool ok() const { return error.code == ErrorCode::None; }

    /// `field` names an array item with "[]" ("color_keys[].at"), filled with `index`.
    void fail(const char* field, int index, const char* why) {
        if (!ok()) return;
        const char* hole = std::strstr(field, "[]");
        if (index >= 0 && hole != nullptr) {
            error = Error::make(ErrorCode::InvalidArgument, "%.*s[%d]%s: %s", static_cast<int>(hole - field), field,
                                index, hole + 2, why);
        } else {
            error = Error::make(ErrorCode::InvalidArgument, "%s: %s", field, why);
        }
    }
    void finite(float v, const char* field, int index = -1) {
        if (!finite_float(v)) fail(field, index, "not finite");
    }
    void within(float v, float lo, float hi, const char* field, const char* why, int index = -1) {
        finite(v, field, index);
        if (ok() && !(v >= lo && v <= hi)) fail(field, index, why);
    }
    void time(FrameTime t, const char* field, int index = -1) {
        if (!t.valid()) fail(field, index, "not a valid FrameTime");
    }
};

void check_emission(const ParticleDesc& d, Checker& c) {
    c.within(d.rate, 0.0f, 491520.0f, "rate", "must be 0..491520 per second");
    c.time(d.delay, "delay");
    c.time(d.duration, "duration");
    for (int i = 0; i < kMaxParticleBursts; ++i) {
        const ParticleBurst& b = d.bursts[i];
        c.time(b.at, "bursts[].at", i);
        c.time(b.interval, "bursts[].interval", i);
        if (b.count.min > b.count.max) c.fail("bursts[].count", i, "min must not exceed max");
        if (b.count.max > kMaxParticles) c.fail("bursts[].count", i, "max must be <= 8192");
        if (b.cycles != 1 && b.interval.valid() && b.interval.ticks < FrameTime::kTicksPerFrame) {
            c.fail("bursts[].interval", i, "must be >= 1 frame when cycles != 1");
        }
    }
}

void check_shape(const ParticleDesc& d, Checker& c) {
    if (static_cast<int>(d.shape) > static_cast<int>(ParticleShape::Line)) c.fail("shape", -1, "unknown shape");
    c.within(d.box.x, 0.0f, 3.0e38f, "box.x", "must be >= 0");
    c.within(d.box.y, 0.0f, 3.0e38f, "box.y", "must be >= 0");
    c.within(d.radius, 0.0f, 3.0e38f, "radius", "must be >= 0");
    c.within(d.inner_radius, 0.0f, d.radius, "inner_radius", "must be 0..radius");
    c.within(d.arc_start, -3.0e38f, 3.0e38f, "arc_start", "not finite");
    c.within(d.arc, 0.0f, kFullTurn, "arc", "must be 0..kFullTurn");
    c.within(d.length, 0.0f, 3.0e38f, "length", "must be >= 0");
}

void check_motion(const ParticleDesc& d, Checker& c) {
    if (static_cast<int>(d.direction) > static_cast<int>(ParticleDirection::Outward)) {
        c.fail("direction", -1, "unknown direction");
    }
    c.finite(d.angle, "angle");
    c.within(d.spread, 0.0f, kFullTurn, "spread", "must be 0..kFullTurn");
    c.finite(d.speed.min, "speed.min");
    c.finite(d.speed.max, "speed.max");
    c.within(d.inherit_velocity, 0.0f, 1.0f, "inherit_velocity", "must be 0..1");
    c.finite(d.gravity.x, "gravity.x");
    c.finite(d.gravity.y, "gravity.y");
    c.within(d.damping, 0.0f, 60.0f, "damping", "must be 0..60 per second");
    c.finite(d.rotation.min, "rotation.min");
    c.finite(d.rotation.max, "rotation.max");
    c.finite(d.spin.min, "spin.min");
    c.finite(d.spin.max, "spin.max");
    c.time(d.lifetime.min, "lifetime.min");
    c.time(d.lifetime.max, "lifetime.max");
    if (c.ok() && d.lifetime.min.ticks < 1) c.fail("lifetime.min", -1, "must be at least 1 tick");
    if (c.ok() && d.lifetime.min.ticks > d.lifetime.max.ticks) c.fail("lifetime", -1, "min must not exceed max");
}

/// Keys: used ones first (`at` ≥ 0), `at` in 0..1, ascending (equal allowed: a step).
template <typename Key>
void check_keys(const Key* keys, const char* at_name, Checker& c) {
    bool unused_seen = false;
    float previous = 0;
    for (int i = 0; i < kMaxParticleKeys; ++i) {
        const float at = keys[i].at;
        c.finite(at, at_name, i);
        if (!c.ok()) return;
        if (at < 0) {
            unused_seen = true;
            continue;
        }
        if (unused_seen) c.fail(at_name, i, "a used key after an unused one");
        if (at > 1) c.fail(at_name, i, "must be 0..1 of the life");
        if (i > 0 && at < previous) c.fail(at_name, i, "keys must be ascending");
        previous = at;
    }
}

void check_appearance(const ParticleDesc& d, Checker& c) {
    c.within(d.size.min, 0.0f, 3.0e38f, "size.min", "must be >= 0");
    c.within(d.size.max, 0.0f, 3.0e38f, "size.max", "must be >= 0");
    check_keys(d.color_keys, "color_keys[].at", c);
    check_keys(d.size_keys, "size_keys[].at", c);
    for (int i = 0; i < kMaxParticleKeys; ++i) {
        if (d.size_keys[i].at >= 0) c.within(d.size_keys[i].size, 0.0f, 3.0e38f, "size_keys[].size", "must be >= 0", i);
    }
    bool unused_seen = false;
    for (int i = 0; i < kMaxParticleFrames; ++i) {
        const ParticleFrame& f = d.frames[i];
        if (f.region.w < 0) {
            unused_seen = true;
            continue;
        }
        if (unused_seen) c.fail("frames[]", i, "a used frame after an unused one");
        if (f.region.x < 0 || f.region.y < 0 || f.region.h < 0) c.fail("frames[].region", i, "x, y, h must be >= 0");
        c.finite(f.pivot.x, "frames[].pivot.x", i);
        c.finite(f.pivot.y, "frames[].pivot.y", i);
    }
    if (static_cast<int>(d.frame_mode) > static_cast<int>(ParticleFrameMode::Rate)) {
        c.fail("frame_mode", -1, "unknown frame mode");
    }
    if (d.frame_repeats < 1) c.fail("frame_repeats", -1, "must be 1..255");
    c.within(d.frame_rate, 0.0f, 600.0f, "frame_rate", "must be 0..600 per second");
}

void check_rest(const ParticleDesc& d, Checker& c) {
    if (static_cast<int>(d.space) > static_cast<int>(ParticleSpace::Local)) c.fail("space", -1, "unknown space");
    if (static_cast<int>(d.order) > static_cast<int>(ParticleOrder::NewestFirst)) c.fail("order", -1, "unknown order");
    if (d.prewarm > kMaxParticlePrewarm) c.fail("prewarm", -1, "must be <= 600 frames");
}

} // namespace

Error check_particle_desc(const ParticleDesc& desc) {
    Checker c;
    check_emission(desc, c);
    check_shape(desc, c);
    check_motion(desc, c);
    check_appearance(desc, c);
    check_rest(desc, c);
    return c.error;
}

} // namespace nnh::detail
