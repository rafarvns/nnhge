// Accuracy (spec 022 FR-3, AC-3, T1.6; plan open question 1): 2000 seeded chains (depth 1..16, world coordinates
// within ±4096 px, accumulated scales ¼..4, any angles) against a double reference. Composition rounding (the reference
// uses the engine's sine values): within 1/32 px. The whole error (the reference uses <cmath>'s sine): measured and
// bounded. NNH_ACCURACY_REPORT=1 prints both worst cases (plan.md "Stage 1 results").

#include "nnh/transform.hpp"
#include "test.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

using nnh::Transform2D;
using nnh::Vec2;
using nnh::WorldTransform2D;

namespace {

struct DMap {
    double xx = 1, xy = 0, yx = 0, yy = 1, ox = 0, oy = 0;
};

/// The child's map in double: parent ∘ (move · rotate · scale), with the given sine and cosine.
DMap compose_d(const DMap& p, const Transform2D& l, double s, double c) {
    const double lxx = c * l.scale.x, lxy = s * l.scale.x, lyx = -s * l.scale.y, lyy = c * l.scale.y;
    DMap m;
    m.xx = p.xx * lxx + p.yx * lxy;
    m.xy = p.xy * lxx + p.yy * lxy;
    m.yx = p.xx * lyx + p.yx * lyy;
    m.yy = p.xy * lyx + p.yy * lyy;
    m.ox = p.xx * l.position.x + p.yx * l.position.y + p.ox;
    m.oy = p.xy * l.position.x + p.yy * l.position.y + p.oy;
    return m;
}

struct Rng {
    std::uint32_t s;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float uniform(float lo, float hi) { return lo + (hi - lo) * static_cast<float>(next() & 0xFFFFFF) / 16777216.0f; }
};

struct Worst {
    double engine_sine = 0, exact_sine = 0;
};

Worst measure() {
    Worst worst;
    Rng rng{0x9E3779B9u};
    for (int chain = 0; chain < 2000; ++chain) {
        const int depth = 1 + static_cast<int>(rng.next() % 16);
        WorldTransform2D w;
        DMap a, b; // a: the engine's sine values; b: <cmath>'s
        double scale = 1;
        for (int level = 0; level < depth; ++level) {
            Transform2D l;
            const float reach = level == 0 ? 400.0f : 40.0f;
            l.position = {rng.uniform(-reach, reach), rng.uniform(-reach, reach)};
            l.rotation = rng.uniform(-12.0f, 12.0f);
            float sx = rng.uniform(0.25f, 4.0f), sy = (rng.next() & 1) != 0 ? sx : rng.uniform(0.25f, 4.0f);
            const double grown = scale * (sx > sy ? sx : sy);
            if (grown > 4.0 || scale * (sx < sy ? sx : sy) < 0.25) sx = sy = 1.0f; // keep the accumulated scale in ¼..4
            scale *= (sx > sy ? sx : sy);
            if ((rng.next() & 7) == 0) sx = -sx; // mirrors
            l.scale = {sx, sy};
            const nnh::SinCos sc = nnh::sin_cos(l.rotation);
            w = level == 0 ? nnh::to_world(l) : nnh::compose(w, l);
            a = compose_d(level == 0 ? DMap{} : a, l, sc.sin, sc.cos);
            b = compose_d(level == 0 ? DMap{} : b, l, std::sin(static_cast<double>(l.rotation)),
                          std::cos(static_cast<double>(l.rotation)));
            const double ea = std::fmax(std::fabs(w.map.origin.x - a.ox), std::fabs(w.map.origin.y - a.oy));
            const double eb = std::fmax(std::fabs(w.map.origin.x - b.ox), std::fabs(w.map.origin.y - b.oy));
            worst.engine_sine = std::fmax(worst.engine_sine, ea);
            worst.exact_sine = std::fmax(worst.exact_sine, eb);
            CHECK(std::fabs(a.ox) <= 4096 && std::fabs(a.oy) <= 4096); // the domain FR-3 states
        }
    }
    if (std::getenv("NNH_ACCURACY_REPORT") != nullptr)
        std::printf("accuracy: worst %.6g px (engine sine), %.6g px (exact sine)\n", worst.engine_sine, worst.exact_sine);
    return worst;
}

} // namespace

NNH_TEST(transform_accuracy_within_a_32nd_of_a_pixel) {
    const Worst w = measure();
    CHECK(w.engine_sine < 1.0 / 32.0);
    CHECK(w.exact_sine < 1.0 / 16.0); // the sine's own error (< 2·10⁻⁶ rad per level) adds; measured in plan.md
}

NNH_TEST(transform_exactness_cases) {
    // Roots bit for bit, unmoved chains as hand sums, quarter turns on whole pixels (also test_compose.cpp).
    const Transform2D root{.position = {-4095.75f, 4095.5f}, .rotation = 3.0f, .scale = {0.25f, -4.0f}};
    const WorldTransform2D w = nnh::to_world(root);
    CHECK(w.position() == root.position && w.rotation == root.rotation && w.scale == root.scale);
    WorldTransform2D q = nnh::to_world({.position = {1000, -1000}});
    for (int i = 0; i < 16; ++i) q = nnh::compose(q, {.position = {3, 1}, .rotation = nnh::kQuarterTurn});
    // Sixteen quarter turns, each step 3 along and 1 across the turned axes: back to whole pixels.
    CHECK(q.position().x == std::floor(q.position().x) && q.position().y == std::floor(q.position().y));
    CHECK(q.map.x == (Vec2{1, 0}) && q.map.y == (Vec2{0, 1}));
}
