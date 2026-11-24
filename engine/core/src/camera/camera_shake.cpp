// Trauma shake (spec 018 FR-8): Eiserloh's trauma² with the engine's own seeded value noise (an integer hash
// and a smoothstep, no libm), so the same seed and calls shake the same on every system.

#include "camera/camera_internal.hpp"
#include "core/mix32.hpp"

namespace nnh::detail::cam {
namespace {

/// The engine's integer finalizer (core/mix32.hpp, shared with the particles' generator since spec 024).
std::uint32_t mix(std::uint32_t x) { return mix32(x); }

constexpr float kTwoPowMinus23 = 1.0f / 8388608.0f;
constexpr float kTwoPowMinus16 = 1.0f / 65536.0f;

} // namespace

std::uint32_t noise_hash(std::uint32_t seed, int axis, std::uint32_t cell) {
    return mix(seed ^ mix(cell * 2u + static_cast<std::uint32_t>(axis)));
}

float lattice_value(std::uint32_t hash) {
    return static_cast<float>(hash >> 8) * kTwoPowMinus23 - 1.0f;
}

float noise(std::uint32_t seed, int axis, std::uint32_t t) {
    const std::uint32_t cell = t >> 16;
    const float v0 = lattice_value(noise_hash(seed, axis, cell));
    const float v1 = lattice_value(noise_hash(seed, axis, cell + 1));
    const float u = static_cast<float>(t & 0xFFFFu) * kTwoPowMinus16;
    const float s = u * u * (3.0f - 2.0f * u);
    return v0 + (v1 - v0) * s;
}

std::uint32_t noise_step(float speed) {
    const auto step = static_cast<std::uint32_t>(speed * 65536.0f + 0.5f);
    return step == 0 ? 1u : step;
}

Vec2 shake_offset(const CameraSettings& s, float trauma, std::uint32_t t) {
    if (!(trauma > 0)) return {};
    const float t2 = trauma * trauma;
    return {(s.max_shake.x / s.zoom) * t2 * noise(s.seed, 0, t),
            (s.max_shake.y / s.zoom) * t2 * noise(s.seed, 1, t)};
}

} // namespace nnh::detail::cam

namespace nnh {

void CameraRig::add_trauma(float amount) {
    if (misuse(amount == amount, "trauma must not be NaN")) return;
    float t = trauma_ + amount;
    if (t > 1) t = 1;
    if (!(t > 0)) t = 0;
    trauma_ = t;
}

void CameraRig::stop_shake() {
    trauma_ = 0;
    final_ = followed_;
}

} // namespace nnh
