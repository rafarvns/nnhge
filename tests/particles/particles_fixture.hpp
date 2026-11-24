#pragma once
// Shared helpers for the particle tests (spec 024): results, misuse runs, checksums of emitters, seeded values.

#include <bit>
#include <cstdint>
#include <cstring>

#include "crash/crash_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/particles.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace particles_test {

/// The value of a Result that must succeed (moved out).
template <typename T>
T take(nnh::Result<T> r) {
    CHECK(r.ok());
    return static_cast<T&&>(r.value());
}

/// An emitter that must be created.
inline nnh::ParticleEmitter make(const nnh::ParticleDesc& d, int capacity = 256) {
    return take(nnh::ParticleEmitter::create(d, capacity));
}

/// Runs `fn` and reports whether it reached the crash screen (an assert).
inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install();
    return nnh::platform_fake::run_expecting_crash(fn);
}

/// Misuse asserts in Debug/Development and is harmless in Release.
inline bool asserts_or_is_ignored(void (*fn)()) { return crashes(fn) == nnh::build::kAssertsEnabled; }

inline std::uint32_t bits(float f) { return std::bit_cast<std::uint32_t>(f); }

/// FNV-1a over 32-bit words.
struct Fnv {
    std::uint32_t h = 2166136261u;
    void word(std::uint32_t w) {
        for (int i = 0; i < 4; ++i) {
            h ^= (w >> (8 * i)) & 0xFFu;
            h *= 16777619u;
        }
    }
    void f(float v) { word(bits(v)); }
};

/// Every live particle's view (and the counters) hashed: equal hashes = the same particles bit for bit.
inline std::uint32_t checksum(const nnh::ParticleEmitter& e) {
    Fnv fnv;
    const nnh::ParticleStats s = e.stats();
    fnv.word(static_cast<std::uint32_t>(s.live));
    fnv.word(s.emitted);
    fnv.word(s.dropped);
    for (int i = 0; i < e.alive(); ++i) {
        const nnh::ParticleView v = e.particle(i);
        fnv.f(v.position.x);
        fnv.f(v.position.y);
        fnv.f(v.velocity.x);
        fnv.f(v.velocity.y);
        fnv.f(v.rotation);
        fnv.f(v.size);
        fnv.word(static_cast<std::uint32_t>(v.color.r | (v.color.g << 8) | (v.color.b << 16) | (v.color.a << 24)));
        fnv.word(static_cast<std::uint32_t>(v.frame));
        fnv.word(static_cast<std::uint32_t>(v.age.ticks));
        fnv.word(static_cast<std::uint32_t>(v.lifetime.ticks));
    }
    return fnv.h;
}

/// Two emitters hold the same particles bit for bit.
inline bool same_particles(const nnh::ParticleEmitter& a, const nnh::ParticleEmitter& b) {
    if (a.alive() != b.alive()) return false;
    for (int i = 0; i < a.alive(); ++i) {
        const nnh::ParticleView x = a.particle(i), y = b.particle(i);
        if (std::memcmp(&x, &y, sizeof x) != 0) return false;
    }
    return true;
}

/// A description that moves: a cone up, speeds, gravity, lifetimes 20..40 frames.
inline nnh::ParticleDesc moving_desc() {
    nnh::ParticleDesc d;
    d.speed = {30, 90};
    d.angle = -nnh::kQuarterTurn;
    d.spread = 1.0f;
    d.gravity = {0, 120};
    d.lifetime = {nnh::Frames{20}, nnh::Frames{40}};
    return d;
}

} // namespace particles_test
