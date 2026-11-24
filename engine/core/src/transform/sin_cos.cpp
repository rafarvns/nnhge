// The public deterministic sine and cosine (spec 022 FR-3): the inline code of sin_cos.hpp, plus the misuse check.

#include "transform/sin_cos.hpp"

#include "nnh/assert.hpp"

namespace nnh {

SinCos sin_cos(float radians) {
    NNH_ASSERT(detail::finite_bits(detail::float_bits(radians)), "nnh::sin_cos(): a non-finite angle");
    return detail::sin_cos_inline(radians);
}

} // namespace nnh
