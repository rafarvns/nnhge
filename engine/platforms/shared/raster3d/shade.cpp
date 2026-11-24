// FR-10's shading for one pixel (raster3d.hpp): surface = texture × vertex colour × base; lit: surface.rgb ×
// (ambient + Σ colourᵢ · max(N·Lᵢ, 0) · attᵢ) + Σ colourᵢ · specular · D0(N·Hᵢ) · attᵢ, clamped; the specular and
// distance factors through the shared 256-entry tables with linear interpolation (as the PICA200 reads them).

#include <cmath>

#include "raster3d.hpp"

namespace nnh::raster3d {
namespace {

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

void unit(float v[3]) {
    const float l2 = (v[0] * v[0] + v[1] * v[1]) + v[2] * v[2];
    if (!(l2 > 1e-30f)) return;
    const float l = std::sqrt(l2);
    v[0] = v[0] / l, v[1] = v[1] / l, v[2] = v[2] / l;
}

float dot3(const float* a, const float* b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }

} // namespace

std::uint8_t to_byte(float c) {
    const float v = std::floor(clamp01(c) * 255.0f + 0.5f);
    return static_cast<std::uint8_t>(v);
}

void shade(const platform::Pass3D& pass, const LightTables& tables, const platform::Material3DGpu& m, bool has_colors,
           const TextureView& texture, const float* attr, float out[4]) {
    float s[4] = {m.base[0], m.base[1], m.base[2], m.base[3]};
    if (m.vertex_colors && has_colors)
        for (int c = 0; c < 4; ++c) s[c] = s[c] * attr[8 + c];
    if (texture.rgba != nullptr) {
        float t[4];
        sample(texture, attr[6], attr[7], m.filter, m.wrap_u, m.wrap_v, t);
        for (int c = 0; c < 4; ++c) s[c] = t[c] * s[c];
    }
    out[3] = clamp01(s[3]);
    if (!m.lit) {
        for (int c = 0; c < 3; ++c) out[c] = clamp01(s[c]);
        return;
    }
    float n[3] = {attr[3], attr[4], attr[5]};
    unit(n);
    float v[3] = {-attr[0], -attr[1], -attr[2]}; // towards the eye
    unit(v);
    float diffuse[3] = {pass.ambient[0], pass.ambient[1], pass.ambient[2]};
    float spec[3] = {0, 0, 0};
    const bool shiny = m.specular[0] > 0.0f || m.specular[1] > 0.0f || m.specular[2] > 0.0f;
    const int level = m.shininess > 7 ? 7 : m.shininess;
    for (int i = 0; i < pass.light_count; ++i) {
        const platform::Light3DGpu& light = pass.lights[i];
        float l[3] = {light.vector[0], light.vector[1], light.vector[2]};
        float att = 1.0f;
        if (light.kind == 1) {
            l[0] = l[0] - attr[0], l[1] = l[1] - attr[1], l[2] = l[2] - attr[2];
            const float d = std::sqrt((l[0] * l[0] + l[1] * l[1]) + l[2] * l[2]);
            att = lut_read(tables.distance, kDistanceEntries, d * light.inv_range);
            unit(l);
        }
        const float nl = dot3(n, l);
        if (nl > 0.0f)
            for (int c = 0; c < 3; ++c) diffuse[c] = diffuse[c] + light.color[c] * (nl * att);
        if (shiny && nl > 0.0f) { // highlights clamped where the light is behind the surface (as the PICA200)
            float h[3] = {l[0] + v[0], l[1] + v[1], l[2] + v[2]};
            unit(h);
            const float f = lut_read(tables.specular[level], kSpecularEntries, dot3(n, h)) * att;
            for (int c = 0; c < 3; ++c) spec[c] = spec[c] + light.color[c] * f;
        }
    }
    for (int c = 0; c < 3; ++c) out[c] = clamp01(s[c] * clamp01(diffuse[c]) + spec[c] * m.specular[c]);
}

} // namespace nnh::raster3d
