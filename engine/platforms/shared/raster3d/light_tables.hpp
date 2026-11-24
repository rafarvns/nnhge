#pragma once
// The lighting lookup tables (spec 026 FR-10, plan P9), shared by the CPU rasteriser (PC, fake) and the 3DS backend so
// both light from the same numbers, laid out as the PICA200 reads them: the specular table per shininess level on a
// signed N·H input (128 entries over [0, 1), x = i / 128; negative inputs give 0), value x^(2^(level+1)) by repeated
// squaring (no libm); the point lights' distance table on an unsigned input (256 entries, x = i / 256), value
// (1 − x)². Each table has one more entry (x = 1) for the linear interpolation between entries. Pure; -ffp-contract=off.

namespace nnh::raster3d {

inline constexpr int kSpecularEntries = 128;
inline constexpr int kDistanceEntries = 256;
inline constexpr int kShininessLevels = 8;

struct LightTables {
    float specular[kShininessLevels][kSpecularEntries + 1]; ///< entry i: (i / 128)^(2^(level + 1))
    float distance[kDistanceEntries + 1];                  ///< entry i: (1 − i / 256)²
};

/// Fills every table (deterministic: the same bits on every system).
void make_light_tables(LightTables& out);

/// The table of `entries` steps over [0, 1] read at x with linear interpolation; x clamped to [0, 1].
float lut_read(const float* table, int entries, float x);

} // namespace nnh::raster3d
