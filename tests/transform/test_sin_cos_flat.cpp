// The branch-free sine (spec 024 T10.3): sin_cos_flat gives sin_cos_inline's bits for every angle — a stride
// through all 2³² float patterns (both signs, subnormals, huge values, NaNs, infinities) and every float near
// the quarter turns, where the snap and the quadrant change.

#include "test.hpp"
#include "transform/sin_cos_flat.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

using nnh::detail::sin_cos_flat;
using nnh::detail::sin_cos_inline;

namespace {

std::uint32_t bits(float f) {
    return std::bit_cast<std::uint32_t>(f);
}

/// The number of angles whose sine or cosine bits differ.
int differences(float a) {
    const nnh::SinCos x = sin_cos_inline(a), y = sin_cos_flat(a);
    return bits(x.sin) != bits(y.sin) || bits(x.cos) != bits(y.cos) ? 1 : 0;
}

} // namespace

NNH_TEST(sin_cos_flat_equals_sin_cos_inline_across_every_float_pattern) {
    int wrong = 0;
    for (std::uint64_t b = 0; b <= 0xFFFFFFFFull; b += 997)
        wrong += differences(std::bit_cast<float>(static_cast<std::uint32_t>(b)));
    for (std::uint32_t b :
         {0x00000000u, 0x80000000u, 0x7F800000u, 0xFF800000u, 0x7FC00000u, 0xFFC00000u, 0x00000001u,
          0x4B000000u, 0xCB000000u, 0x4AFFFFFFu, 0x3F000000u, 0x7F7FFFFFu, 0xFF7FFFFFu}) {
        wrong += differences(std::bit_cast<float>(b));
    }
    CHECK(wrong == 0);
}

NNH_TEST(sin_cos_flat_equals_sin_cos_inline_around_quarter_turns) {
    int wrong = 0;
    for (int k = -64; k <= 64; ++k) {
        const std::uint32_t centre = bits(static_cast<float>(k) * nnh::kQuarterTurn);
        // ±16 000 floats: out to |f| ≈ 0.0012, past where sin_quarter(1 − |f|) rounds to just below 1 (the
        // clamp's edge)
        for (int d = -16000; d <= 16000; ++d)
            wrong += differences(std::bit_cast<float>(centre + static_cast<std::uint32_t>(d)));
    }
    for (int i = 0; i < 200'000; ++i)
        wrong += differences((static_cast<float>(i) - 100'000.0f) * 0.000613f);
    CHECK(wrong == 0);
}

NNH_TEST(sin_cos_flat2_gives_each_lane_the_single_angle_bits) {
    int wrong = 0;
    for (int i = 0; i < 100'000; ++i) {
        const float a = (static_cast<float>(i) - 50'000.0f) * 0.00137f;
        const float b = std::bit_cast<float>(0x9E3779B9u * static_cast<std::uint32_t>(i)); // any pattern
        nnh::SinCos x, y;
        nnh::detail::sin_cos_flat2(a, b, x, y);
        const nnh::SinCos p = sin_cos_inline(a), q = sin_cos_inline(b);
        wrong += bits(x.sin) != bits(p.sin) || bits(x.cos) != bits(p.cos) || bits(y.sin) != bits(q.sin) ||
                 bits(y.cos) != bits(q.cos);
    }
    CHECK(wrong == 0);
}
