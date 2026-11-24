// The engine's deterministic sine polynomial, out of line (spec 015 FR-3; moved by spec 022 T1.1, bits unchanged).

#include "core/det_sine.hpp"

namespace nnh::detail {

float sin_quarter(float u) { return sin_quarter_inline(u); }

} // namespace nnh::detail
