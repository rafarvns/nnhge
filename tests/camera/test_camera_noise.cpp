// The shake's noise (spec 018 FR-8, FR-12): an integer hash, exact lattice values in [-1, 1), a smoothstep
// between them on a 16.16 clock; golden values pin the bits on every compiler and system.

#include "camera/camera_internal.hpp"
#include "camera_fixture.hpp"

#include <cstdio>

namespace cam = nnh::detail::cam;

NNH_TEST(lattice_values_are_exact_and_cover_minus_one_to_one) {
    CHECK(cam::lattice_value(0) == -1.0f);
    CHECK(cam::lattice_value(0x80000000u) == 0.0f);
    CHECK(cam::lattice_value(0xFFFFFFFFu) == 1.0f - 1.0f / 8388608.0f);
    CHECK(cam::lattice_value(0x00000100u) == -1.0f + 1.0f / 8388608.0f); // one step of 2^-23
}

NNH_TEST(noise_at_a_whole_cell_is_its_lattice_value) {
    for (std::uint32_t cell = 0; cell < 50; ++cell) {
        const float n = cam::noise(1234, 0, cell << 16);
        CHECK(n == cam::lattice_value(cam::noise_hash(1234, 0, cell)));
    }
}

NNH_TEST(noise_stays_in_minus_one_to_one) {
    for (std::uint32_t t = 0; t < 400u * 65536u; t += 4099) {
        const float x = cam::noise(7, 0, t);
        const float y = cam::noise(7, 1, t);
        CHECK(x >= -1 && x <= 1 && y >= -1 && y <= 1);
    }
}

NNH_TEST(noise_is_continuous_its_change_per_tick_is_bounded) {
    // The smoothstep's slope is at most 1.5 per cell and lattice values differ by < 2: |Δ| ≤ 3 × Δt (cells).
    const std::uint32_t step = 4096; // 1/16 cell
    float prev = cam::noise(99, 1, 0);
    for (std::uint32_t t = step; t < 200u * 65536u; t += step) {
        const float n = cam::noise(99, 1, t);
        const float d = n > prev ? n - prev : prev - n;
        CHECK(d <= 3.0f / 16 + 1e-6f);
        prev = n;
    }
}

NNH_TEST(the_same_seed_gives_the_same_noise_and_other_seeds_or_axes_differ) {
    int differ_seed = 0, differ_axis = 0;
    for (std::uint32_t t = 0; t < 100u * 65536u; t += 32768) {
        CHECK(cam_test::same_bits(cam::noise(5, 0, t), cam::noise(5, 0, t)));
        if (cam::noise(5, 0, t) != cam::noise(6, 0, t)) ++differ_seed;
        if (cam::noise(5, 0, t) != cam::noise(5, 1, t)) ++differ_axis;
    }
    CHECK(differ_seed > 190 && differ_axis > 190);
}

NNH_TEST(noise_step_is_the_speed_in_16_16_cells) {
    CHECK(cam::noise_step(0.5f) == 32768u);
    CHECK(cam::noise_step(1) == 65536u);
    CHECK(cam::noise_step(0.25f) == 16384u);
    CHECK(cam::noise_step(1e-9f) == 1u); // never stands still
}

NNH_TEST(noise_golden_values) {
    struct Case {
        std::uint32_t seed;
        int axis;
        std::uint32_t t;
        std::uint32_t bits;
    };
    const Case cases[] = {
        {0x4E4E4843u, 0, 0, 0xBC1EA880u},     {0x4E4E4843u, 1, 0, 0xBE92EDC0u},
        {0x4E4E4843u, 0, 98304, 0x3E61E538u}, {0x4E4E4843u, 1, 1234567, 0x3F796907u},
        {1, 0, 0xFFFF0000u, 0xBF3EFC4Eu},     {0xDEADBEEFu, 1, 777777, 0x3EA5B466u},
    };
    for (const Case& c : cases) {
        const float n = cam::noise(c.seed, c.axis, c.t);
        if (std::bit_cast<std::uint32_t>(n) != c.bits)
            std::printf("noise(0x%08X, %d, %u) = 0x%08Xu\n", static_cast<unsigned>(c.seed), c.axis,
                        static_cast<unsigned>(c.t), std::bit_cast<std::uint32_t>(n));
        CHECK(std::bit_cast<std::uint32_t>(n) == c.bits);
    }
}
