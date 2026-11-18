// Analog stick shaping (stick.hpp).

#include "input/stick.hpp"

#include <cmath>

namespace nnh::detail {

Stick shape_stick(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return {};
    const float length = std::sqrt(x * x + y * y);
    if (length <= kStickDeadZone) return {};
    float shaped = (length - kStickDeadZone) / (1.0f - kStickDeadZone);
    if (shaped > 1.0f) shaped = 1.0f;
    const float scale = shaped / length;
    return Stick{x * scale, y * scale};
}

} // namespace nnh::detail
