// Appearance over life (spec 024 FR-12; T3.2): colour and size curves exact at their keys, within 1/256 of a double
// reference between them (flat and steep), zero or one key constant, mul8 exact over every byte pair.

#include <cmath>

#include "particles/particle_appearance.hpp"
#include "particles_fixture.hpp"

using namespace nnh::detail;
using nnh::Color;
using nnh::ParticleColorKey;
using nnh::ParticleSizeKey;

namespace {

/// The exact piecewise-linear value of channel `k` at t.
double reference(const ParticleColorKey* keys, int n, double t, int k) {
    auto ch = [&](int i) {
        const Color c = keys[i].color;
        const std::uint8_t v[4] = {c.r, c.g, c.b, c.a};
        return static_cast<double>(v[k]);
    };
    if (t <= keys[0].at) return ch(0);
    for (int i = 0; i + 1 < n; ++i) {
        if (t < keys[i + 1].at) {
            const double a = keys[i].at, b = keys[i + 1].at;
            return b > a ? ch(i) + (ch(i + 1) - ch(i)) * (t - a) / (b - a) : ch(i + 1);
        }
    }
    return ch(n - 1);
}

std::uint32_t channel(std::uint32_t packed, int k) { return (packed >> (8 * k)) & 0xFFu; }

} // namespace

NNH_TEST(mul8_is_round_a_times_b_over_255_for_every_pair) {
    for (std::uint32_t a = 0; a < 256; ++a) {
        for (std::uint32_t b = 0; b < 256; ++b) {
            CHECK(mul8(a, b) == (a * b + 127) / 255);
        }
    }
}

NNH_TEST(colour_curve_is_exact_at_its_keys) {
    const ParticleColorKey keys[4] = {{0, nnh::rgba(255, 255, 0, 255)},
                                      {0.3f, nnh::rgba(255, 128, 0, 255)},
                                      {0.7f, nnh::rgba(128, 0, 0, 200)},
                                      {1, nnh::rgba(0, 0, 0, 0)}};
    ParticleColorCurve c;
    build_color_curve(keys, 4, c);
    for (const ParticleColorKey& key : keys) {
        if (key.at >= 1) continue; // t stays below 1
        const std::uint32_t v = color_at(c, key.at, 0xFFFFFFFFu);
        CHECK(v == pack_color(key.color));
    }
}

NNH_TEST(colour_curve_is_within_1_256_between_keys_flat_and_steep) {
    const ParticleColorKey keys[5] = {{0.1f, nnh::rgba(0, 10, 200, 255)},
                                      {0.1001f, nnh::rgba(255, 12, 0, 0)}, // steep
                                      {0.5f, nnh::rgba(250, 14, 3, 10)},   // flat
                                      {0.5f, nnh::rgba(0, 255, 255, 255)}, // a step
                                      {0.9f, nnh::rgba(77, 88, 99, 111)}};
    ParticleColorCurve c;
    build_color_curve(keys, 5, c);
    for (int i = 0; i < 100000; ++i) {
        const float t = static_cast<float>(i) / 100000.0f;
        const std::uint32_t v = color_at(c, t, 0xFFFFFFFFu);
        for (int k = 0; k < 4; ++k) {
            const double exact = reference(keys, 5, t, k);
            CHECK(std::fabs(static_cast<double>(channel(v, k)) - exact) / 255.0 <= 1.0 / 256.0);
        }
    }
}

NNH_TEST(colour_curve_with_no_key_is_white_and_one_key_constant) {
    ParticleColorCurve none, one;
    build_color_curve(nullptr, 0, none);
    const ParticleColorKey key{0.4f, nnh::rgba(1, 2, 3, 4)};
    build_color_curve(&key, 1, one);
    for (float t : {0.0f, 0.39f, 0.4f, 0.99f}) {
        CHECK(color_at(none, t, pack_color(nnh::rgba(9, 8, 7, 6))) == pack_color(nnh::rgba(9, 8, 7, 6)));
        CHECK(color_at(one, t, 0xFFFFFFFFu) == pack_color(nnh::rgba(1, 2, 3, 4)));
    }
}

NNH_TEST(colour_curve_multiplies_the_start_colour) {
    const ParticleColorKey key{0, nnh::rgba(128, 255, 0, 64)};
    ParticleColorCurve c;
    build_color_curve(&key, 1, c);
    const std::uint32_t v = color_at(c, 0.5f, pack_color(nnh::rgba(255, 100, 50, 255)));
    CHECK(v == pack_color(nnh::rgba(128, 100, 0, 64)));
}

NNH_TEST(size_curve_exact_at_keys_and_within_bound_between) {
    const ParticleSizeKey keys[3] = {{0, 0.5f}, {0.25f, 2}, {1, 0}};
    ParticleSizeCurve c;
    build_size_curve(keys, 3, c);
    CHECK(size_at(c, 0, 4) == 2 && size_at(c, 0.25f, 4) == 8);
    for (int i = 0; i < 1000; ++i) {
        const float t = static_cast<float>(i) / 1000.0f;
        const double exact = t < 0.25 ? 0.5 + 1.5 * t / 0.25 : 2 - 2 * (t - 0.25) / 0.75;
        CHECK(std::fabs(size_at(c, t, 1) - exact) < 1e-5);
    }
    ParticleSizeCurve none;
    build_size_curve(nullptr, 0, none);
    CHECK(size_at(none, 0.7f, 3) == 3);
}

NNH_TEST(particle_view_colour_and_size_change_over_life) {
    nnh::ParticleDesc d;
    d.lifetime = {nnh::Frames{100}, nnh::Frames{100}};
    d.color_keys[0] = {0, nnh::rgba(255, 0, 0, 255)};
    d.color_keys[1] = {1, nnh::rgba(0, 0, 255, 0)};
    d.size = {4, 4};
    d.size_keys[0] = {0, 1};
    d.size_keys[1] = {1, 0};
    nnh::ParticleEmitter e = particles_test::make(d, 4);
    e.emit(1);
    for (int i = 0; i < 50; ++i) e.update();
    const nnh::ParticleView v = e.particle(0);
    CHECK(v.color.r >= 127 && v.color.r <= 128 && v.color.g == 0 && v.color.b >= 127 && v.color.b <= 128);
    CHECK(v.color.a >= 127 && v.color.a <= 128 && std::fabs(v.size - 2) < 1e-5f); // half way
}
