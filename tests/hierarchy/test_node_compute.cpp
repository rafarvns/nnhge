// The pass's finiteness test (plan 022 T10.3): local_finite() reads the local's words as integers and tests every
// exponent at once; it must answer exactly as is_finite_bits() for every exponent, sign and mantissa in every field.

#include <cstring>

#include "hierarchy/node_compute.hpp"
#include "test.hpp"

using nnh::Node;
using nnh::Transform2D;
using nnh::detail::is_finite_bits;
using nnh::detail::local_finite;

namespace {

/// A node whose local is finite except field `field` (0..4: position x, y, rotation, scale x, y) set to `bits`.
Node with_field(int field, std::uint32_t bits) {
    Node n(Transform2D{.position = {3, -2}, .rotation = 0.5f, .scale = {2, -1}});
    float* fields[] = {&n.local.position.x, &n.local.position.y, &n.local.rotation, &n.local.scale.x, &n.local.scale.y};
    std::memcpy(fields[field], &bits, sizeof bits);
    return n;
}

} // namespace

NNH_TEST(local_finite_agrees_with_is_finite_bits_for_every_exponent_in_every_field) {
    const std::uint32_t mantissas[] = {0u, 1u, 0x400000u, 0x7FFFFFu};
    int mismatches = 0, non_finite = 0;
    for (int field = 0; field < 5; ++field)
        for (std::uint32_t exponent = 0; exponent < 256; ++exponent)
            for (const std::uint32_t mantissa : mantissas)
                for (std::uint32_t sign = 0; sign < 2; ++sign) {
                    const Node n = with_field(field, (sign << 31) | (exponent << 23) | mantissa);
                    mismatches += local_finite(n) != is_finite_bits(n.local) ? 1 : 0;
                    non_finite += local_finite(n) ? 0 : 1;
                }
    CHECK(mismatches == 0);
    CHECK(non_finite == 5 * 4 * 2); // exponent 255 only: infinities and NaNs
}

NNH_TEST(local_finite_is_true_when_every_field_has_the_largest_finite_exponent) {
    Node n;
    const std::uint32_t largest = 0xFF7FFFFFu; // −FLT_MAX: exponent 254, sign set
    float* fields[] = {&n.local.position.x, &n.local.position.y, &n.local.rotation, &n.local.scale.x, &n.local.scale.y};
    for (float* f : fields) std::memcpy(f, &largest, sizeof largest);
    CHECK(local_finite(n));
    CHECK(is_finite_bits(n.local));
}
