// The lighting lookup tables (light_tables.hpp).

#include "light_tables.hpp"

namespace nnh::raster3d {

void make_light_tables(LightTables& out) {
    for (int i = 0; i < kLutSize; ++i) {
        const float x = static_cast<float>(i) / 255.0f;
        float p = x * x; // level 0: x²
        for (int level = 0; level < kShininessLevels; ++level) {
            out.specular[level][i] = p;
            p = p * p;
        }
        const float d = 1.0f - x;
        out.distance[i] = d * d;
    }
}

float lut_read(const float* table, float x) {
    if (!(x > 0.0f)) return table[0];
    if (x >= 1.0f) return table[kLutSize - 1];
    const float f = x * 255.0f;
    const int i = static_cast<int>(f);
    const float t = f - static_cast<float>(i);
    return table[i] + (table[i + 1] - table[i]) * t;
}

} // namespace nnh::raster3d
