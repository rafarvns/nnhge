// Layer numbers out of range (spec 019 FR-1): collision::layer() calls this only for a bad number, so a
// constant expression with one doesn't compile and a run-time one asserts.

#include "nnh/assert.hpp"
#include "nnh/collision.hpp"

namespace nnh::collision::detail {

Layers bad_layer(int n) {
    NNH_ASSERT(false, "collision::layer(%d): layers are numbered 0..31", n);
    (void)n;
    return kNoLayers;
}

} // namespace nnh::collision::detail
