// Fragment lighting (video3d_3ds.hpp, FR-10/FR-11, plan P9): one light environment with 4 lights; the specular
// table D0 on N·H (signed input, 0 below 0) per shininess level and the distance table per point light, both filled
// from the tables the simulator uses (shared/raster3d/light_tables.cpp); highlights clamped where N·L ≤ 0; material
// ambient and diffuse 1, specular 0 = the draw's specular colour. Tables re-selected only when the level changes.

#include <cstring>
#include <new>

#include "light_tables.hpp"
#include "video3d_3ds.hpp"

namespace nnh::platform::ctr {
namespace {

struct Lights {
    C3D_LightEnv env;
    C3D_Light lights[kMaxLights3D];
    C3D_LightLut specular[raster3d::kShininessLevels];
    C3D_LightLutDA distance[kMaxLights3D];
    raster3d::LightTables tables;
    int level = -1;            ///< the D0 table selected now
    float spec[3] = {-1, -1, -1};
    bool bound = false;
};
Lights* g = nullptr;

/// citro3d's LUT input: 256 values then 256 differences to the next entry.
void fill_unsigned(C3D_LightLut& lut, const float* table) { // x = i / 256 for i = 0..256
    float data[512];
    for (int i = 0; i < 256; ++i) {
        data[i] = table[i];
        data[i + 256] = table[i + 1] - table[i];
    }
    LightLut_FromArray(&lut, data);
}

void fill_signed(C3D_LightLut& lut, const float* table) { // entries 0..127: x = i / 128; 128..255: negative x → 0
    float data[512];
    std::memset(data, 0, sizeof data);
    for (int i = 0; i < 128; ++i) {
        data[i] = table[i];
        data[i + 256] = table[i + 1] - table[i];
    }
    LightLut_FromArray(&lut, data);
}

} // namespace

bool lights3d_init() {
    g = new (std::nothrow) Lights;
    if (g == nullptr) return false;
    raster3d::make_light_tables(g->tables);
    for (int l = 0; l < raster3d::kShininessLevels; ++l) fill_signed(g->specular[l], g->tables.specular[l]);
    C3D_LightEnvInit(&g->env);
    C3D_LightEnvClampHighlights(&g->env, true);
    for (int i = 0; i < kMaxLights3D; ++i) {
        C3D_LightInit(&g->lights[i], &g->env);
        fill_unsigned(g->distance[i].lut, g->tables.distance);
        g->distance[i].bias = 0;
        g->distance[i].scale = 1;
        C3D_LightEnable(&g->lights[i], false);
    }
    return true;
}

void lights3d_begin(const Pass3D& pass) {
    C3D_LightEnvAmbient(&g->env, pass.ambient[0], pass.ambient[1], pass.ambient[2]);
    for (int i = 0; i < kMaxLights3D; ++i) {
        C3D_Light& light = g->lights[i];
        const bool on = i < pass.light_count;
        C3D_LightEnable(&light, on);
        if (!on) continue;
        const Light3DGpu& l = pass.lights[i];
        C3D_FVec pos = FVec4_New(l.vector[0], l.vector[1], l.vector[2], l.kind == 1 ? 1.0f : 0.0f);
        C3D_LightPosition(&light, &pos);
        C3D_LightColor(&light, l.color[0], l.color[1], l.color[2]);
        C3D_LightDistAttnEnable(&light, l.kind == 1);
        if (l.kind == 1) {
            g->distance[i].scale = l.inv_range;
            C3D_LightDistAttn(&light, &g->distance[i]);
        }
    }
    g->level = -1;
    g->spec[0] = -1;
}

void lights3d_material(const Material3DGpu& m) {
    if (!m.lit) {
        if (g->bound) C3D_LightEnvBind(nullptr);
        g->bound = false;
        return;
    }
    if (m.specular[0] != g->spec[0] || m.specular[1] != g->spec[1] || m.specular[2] != g->spec[2]) {
        const C3D_Material mat = {{1, 1, 1}, {1, 1, 1}, {m.specular[0], m.specular[1], m.specular[2]}, {0, 0, 0},
                                  {0, 0, 0}};
        C3D_LightEnvMaterial(&g->env, &mat);
        std::memcpy(g->spec, m.specular, sizeof g->spec);
    }
    const int level = m.shininess > 7 ? 7 : m.shininess;
    if (level != g->level) {
        C3D_LightEnvLut(&g->env, GPU_LUT_D0, GPU_LUTINPUT_NH, true, &g->specular[level]); // signed N·H
        g->level = level;
    }
    if (!g->bound) C3D_LightEnvBind(&g->env);
    g->bound = true;
}

void lights3d_end() {
    C3D_LightEnvBind(nullptr);
    g->bound = false;
}

} // namespace nnh::platform::ctr
