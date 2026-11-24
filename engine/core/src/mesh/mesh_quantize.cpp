// Quantisation (mesh_quantize.hpp). Float→int conversions only after clamping, so no conversion overflows.

#include "mesh/mesh_quantize.hpp"

#include <bit>

namespace nnh::detail::mesh {
namespace {

constexpr float kMinHalf = 9.5367432e-7f; // 2⁻²⁰

/// floor(x) for |x| ≤ 2²³ (the caller clamps first): truncation, one down for negative fractions.
std::int32_t floor_int(float x) {
    const auto t = static_cast<std::int32_t>(x);
    return static_cast<float>(t) > x ? t - 1 : t;
}

bool usable_length2(float l2) {
    const std::uint32_t e = (std::bit_cast<std::uint32_t>(l2) >> 23) & 0xFFu;
    return e != 0xFFu && e >= 27u; // finite, ≥ 2⁻¹⁰⁰
}

} // namespace

Axis axis_for(float lo, float hi) {
    float half = (hi - lo) * 0.5f;
    if (!(half > kMinHalf)) half = kMinHalf;
    return {half / 32767.0f, (lo + hi) * 0.5f};
}

std::int16_t quantize(float v, Axis axis) {
    float x = (v - axis.offset) / axis.scale + 0.5f;
    if (!(x < 32767.0f)) x = 32767.0f; // NaN clamps up too (never reached: inputs are checked)
    if (x < -32767.0f) x = -32767.0f;
    return static_cast<std::int16_t>(floor_int(x));
}

float dequantize(std::int16_t q, Axis axis) { return static_cast<float>(q) * axis.scale + axis.offset; }

void quantize_normal(Vec3 n, std::int8_t out[3]) {
    const float l2 = (n.x * n.x + n.y * n.y) + n.z * n.z;
    if (!usable_length2(l2)) {
        out[0] = 0, out[1] = 127, out[2] = 0;
        return;
    }
    const float l = __builtin_sqrtf(l2);
    const float c[3] = {n.x / l, n.y / l, n.z / l};
    for (int i = 0; i < 3; ++i) {
        float x = c[i] * 127.0f + 0.5f;
        if (x > 127.0f) x = 127.0f;
        if (x < -127.0f) x = -127.0f;
        out[i] = static_cast<std::int8_t>(floor_int(x));
    }
}

Vec3 face_normal(Vec3 a, Vec3 b, Vec3 c) {
    const Vec3 u = b - a, v = c - a;
    const Vec3 n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    const float l2 = (n.x * n.x + n.y * n.y) + n.z * n.z;
    if (!usable_length2(l2)) return {0, 1, 0};
    const float l = __builtin_sqrtf(l2);
    return {n.x / l, n.y / l, n.z / l};
}

} // namespace nnh::detail::mesh
