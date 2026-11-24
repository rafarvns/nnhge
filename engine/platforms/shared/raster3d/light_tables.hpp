#pragma once
// The lighting lookup tables (spec 026 FR-10, plan P9), shared by the CPU rasteriser (PC, fake) and the 3DS backend so
// both light from the same numbers: the specular table per shininess level (x^(2^(level+1)) for x = N·H in [0, 1],
// 256 entries, by repeated squaring: no libm) and the point lights' distance table ((1 − x)² for x = d / range).
// Read with linear interpolation between entries, as the PICA200 does. Pure; -ffp-contract=off.

namespace nnh::raster3d {

inline constexpr int kLutSize = 256;
inline constexpr int kShininessLevels = 8;

struct LightTables {
    float specular[kShininessLevels][kLutSize]; ///< entry i: (i / 255)^(2^(level + 1))
    float distance[kLutSize];                  ///< entry i: (1 − i / 255)²
};

/// Fills every table (deterministic: the same bits on every system).
void make_light_tables(LightTables& out);

/// table[x × 255] with linear interpolation; x clamped to [0, 1].
float lut_read(const float* table, float x);

} // namespace nnh::raster3d
