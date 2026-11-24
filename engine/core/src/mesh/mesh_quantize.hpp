#pragma once
// Quantisation of mesh vertices (spec 026 FR-4, D42), shared by core and nnh-assetc so both write the same bytes. Pure;
// -ffp-contract=off (engine/core/CMakeLists.txt, tools/assetc/CMakeLists.txt).
//
// Positions: offset = the box's centre, scale = max(half extent, 2⁻²⁰) / 32767 per axis,
// q = clamp(floor((p − offset) / scale + 0.5), ±32767); error ≤ half a step (extent / 65534). Normals: unit first, then
// q = floor(n × 127 + 0.5), error ≤ 0.5°. Texture coordinates: the same rule as positions over their own range.

#include <cstdint>

#include "nnh/math3d.hpp"

namespace nnh::detail::mesh {

/// A per-axis decode: value = q × scale + offset.
struct Axis {
    float scale = 0;
    float offset = 0;
};

/// The decode of values in [lo, hi].
Axis axis_for(float lo, float hi);

/// round-half-up((v − offset) / scale), clamped to ±32767.
std::int16_t quantize(float v, Axis axis);
/// q × scale + offset (what the GPU computes).
float dequantize(std::int16_t q, Axis axis);

/// A unit (or zero) normal as int8 components; a zero, tiny or non-finite normal gives (0, 127, 0) (up).
void quantize_normal(Vec3 n, std::int8_t out[3]);

/// The unit normal of triangle (a, b, c), counter-clockwise front; degenerate triangles give (0, 1, 0).
Vec3 face_normal(Vec3 a, Vec3 b, Vec3 c);

} // namespace nnh::detail::mesh
