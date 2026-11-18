// 3DS texture size rule, enforced on every system (gfx/texture_rules.hpp, spec 004 FR-3).

#include "gfx/texture_rules.hpp"
#include "test.hpp"

using nnh::detail::valid_texture_size;

NNH_TEST(texture_sides_that_are_powers_of_two_from_8_to_1024_are_valid) {
    for (int side = 8; side <= 1024; side *= 2) CHECK(valid_texture_size(side, side));
    CHECK(valid_texture_size(8, 1024));
    CHECK(valid_texture_size(256, 16));
}

NNH_TEST(texture_sides_that_are_not_powers_of_two_are_invalid) {
    CHECK(!valid_texture_size(100, 64));
    CHECK(!valid_texture_size(64, 60));
    CHECK(!valid_texture_size(24, 24));
}

NNH_TEST(texture_sides_out_of_range_are_invalid) {
    CHECK(!valid_texture_size(4, 8));
    CHECK(!valid_texture_size(8, 2048));
    CHECK(!valid_texture_size(0, 8));
    CHECK(!valid_texture_size(-8, 8));
}
