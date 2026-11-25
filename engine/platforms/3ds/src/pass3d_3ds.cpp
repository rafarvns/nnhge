// Nintendo 3DS: the 3D pass (contract nnh/platform/video3d.hpp; plan P22): between citro2d's draws of the top screen,
// citro2d's batch is flushed, the mesh shader bound, depth test GREATER with writes (the frame-start clear left 0),
// the lights set; each draw sets its matrices, the rest of its state (vertex buffer, fixed attributes, uv decode and
// colour scale, texture, combiners, lighting, culling, alpha test or blending) only when it changes (pass_cache3d.hpp;
// the optimisation round of 026a: citro3d resends what it is given), and draws its index range; end_3d restores every
// texture parameter it touched and the 2D state.

#include <3ds.h>
#include <citro2d.h>

#include "context_3ds.hpp"
#include "pass_cache3d.hpp"
#include "video3d_3ds.hpp"

namespace nnh::platform {
namespace {

constexpr int kMaxTouched = 32;

struct PassState {
    int mode = -1;            ///< combiner setup: textured × lit (−1 = unknown)
    int cull = -1;
    int alpha = -1;
    int cutoff = -1;
    std::uint8_t flags = 0xFF; ///< attribute layout in use
    C3D_Tex* touched[kMaxTouched]; ///< textures whose filter/wrap we changed, and their parameters
    u32 saved[kMaxTouched];
    int touched_count = 0;
    ctr::PassCache3D cache;    ///< vertex buffer, fixed attributes, constants and texture last sent
};
PassState g;

void combiners(bool textured, bool lit) {
    C3D_TexEnv* e0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(e0);
    if (textured) {
        C3D_TexEnvSrc(e0, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(e0, C3D_Both, GPU_MODULATE);
    } else {
        C3D_TexEnvSrc(e0, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(e0, C3D_Both, GPU_REPLACE);
    }
    C3D_TexEnv* e1 = C3D_GetTexEnv(1);
    C3D_TexEnv* e2 = C3D_GetTexEnv(2);
    C3D_TexEnvInit(e1);
    C3D_TexEnvInit(e2);
    if (lit) { // surface × (ambient + diffuse), then + specular; alpha from the surface
        C3D_TexEnvSrc(e1, C3D_RGB, GPU_PREVIOUS, GPU_FRAGMENT_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(e1, C3D_RGB, GPU_MODULATE);
        C3D_TexEnvSrc(e2, C3D_RGB, GPU_PREVIOUS, GPU_FRAGMENT_SECONDARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(e2, C3D_RGB, GPU_ADD);
    }
    for (int i = 3; i < 6; ++i) C3D_TexEnvInit(C3D_GetTexEnv(i));
}

void bind_texture(TextureHandle handle, const Material3DGpu& m) {
    C3D_Tex* tex = ctr::texture_of(handle);
    if (tex == nullptr) return;
    const std::uint32_t params = static_cast<std::uint32_t>(m.filter) | (static_cast<std::uint32_t>(m.wrap_u) << 8) |
                                 (static_cast<std::uint32_t>(m.wrap_v) << 16);
    if (!g.cache.texture(reinterpret_cast<std::uintptr_t>(tex), params)) return; // bound with these parameters
    bool known = false;
    for (int i = 0; i < g.touched_count; ++i) known = known || g.touched[i] == tex;
    if (!known && g.touched_count < kMaxTouched) {
        g.touched[g.touched_count] = tex;
        g.saved[g.touched_count++] = tex->param;
    }
    const GPU_TEXTURE_FILTER_PARAM f = m.filter == 1 ? GPU_LINEAR : GPU_NEAREST;
    const GPU_TEXTURE_WRAP_PARAM wraps[3] = {GPU_REPEAT, GPU_CLAMP_TO_EDGE, GPU_MIRRORED_REPEAT};
    C3D_TexSetFilter(tex, f, f);
    C3D_TexSetWrap(tex, wraps[m.wrap_u > 2 ? 0 : m.wrap_u], wraps[m.wrap_v > 2 ? 0 : m.wrap_v]);
    C3D_TexBind(0, tex);
}

void material_state(const Material3DGpu& m) {
    const int mode = (m.texture != 0 ? 1 : 0) | (m.lit ? 2 : 0);
    if (mode != g.mode) combiners(m.texture != 0, m.lit), g.mode = mode;
    if (m.texture != 0) bind_texture(m.texture, m);
    ctr::lights3d_material(m);
    const int cull = m.cull_back ? 1 : 0;
    if (cull != g.cull) C3D_CullFace(m.cull_back ? GPU_CULL_BACK_CCW : GPU_CULL_NONE), g.cull = cull;
    if (m.alpha != g.alpha || (m.alpha == 1 && m.cutoff != g.cutoff)) {
        C3D_AlphaTest(m.alpha == 1, GPU_GEQUAL, m.cutoff);
        if (m.alpha == 2) {
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA,
                           GPU_ONE_MINUS_SRC_ALPHA);
            C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_COLOR); // blended: tested, not written
        } else {
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
            C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
        }
        g.alpha = m.alpha, g.cutoff = m.cutoff;
    }
}

void upload_rows(int location, const float* rows, int count, int columns) {
    for (int r = 0; r < count; ++r) {
        const float* v = rows + r * columns;
        C3D_FVUnifSet(GPU_VERTEX_SHADER, location + r, v[0], v[1], v[2], columns == 4 ? v[3] : 0.0f);
    }
}

} // namespace

void begin_3d(Screen screen, const Pass3D& pass) {
    if (!ctr::video3d_ready() || screen != Screen::Top) return;
    ctr::draw_on(Screen::Top);
    C2D_Flush(); // the 2D drawn below the world goes first
    ctr::Shader3D& s = ctr::shader3d();
    C3D_BindProgram(&s.program);
    C3D_Mtx projection;
    ctr::pica_projection(pass.projection, projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s.projection, &projection);
    ctr::lights3d_begin(pass);
    g = PassState{};
}

void draw_3d(const Draw3D& d) {
    if (!ctr::video3d_ready() || d.mesh == 0 || d.index_count == 0) return;
    const auto* mesh = reinterpret_cast<const ctr::Mesh3D*>(d.mesh);
    ctr::Shader3D& s = ctr::shader3d();
    const Material3DGpu& m = d.material;
    // Without vertex colours the colour attribute is fixed at 255 (the loaders stop before it; the stride skips it).
    const std::uint8_t flags = static_cast<std::uint8_t>(mesh->flags & (m.vertex_colors ? 3 : 1));
    if (flags != g.flags) {
        C3D_SetAttrInfo(&s.attrs[flags]);
        g.flags = flags;
    }
    if (g.cache.vertex_buffer(d.mesh, flags)) {
        C3D_BufInfo* buf = C3D_GetBufInfo();
        BufInfo_Init(buf);
        const int loaders = 2 + ((flags & 1) != 0 ? 1 : 0) + ((flags & 2) != 0 ? 1 : 0);
        BufInfo_Add(buf, mesh->vertices, mesh->stride, loaders, loaders == 2 ? 0x10 : (loaders == 3 ? 0x210 : 0x3210));
    }
    if ((flags & 1) == 0 && g.cache.fixed_attribute(2)) C3D_FixedAttribSet(2, 0, 0, 0, 0);
    if ((flags & 2) == 0 && g.cache.fixed_attribute(3)) C3D_FixedAttribSet(3, 255, 255, 255, 255);
    upload_rows(s.model_view, d.model_view, 3, 4);
    upload_rows(s.normal, d.normal, 3, 3);
    // uvDecode and colorScale follow normalMtx: sent together (or not at all) the matrices stay one uniform run.
    // The engine's v is 0 at the image's top (glTF); citro3d samples v = 1 there (texture_3ds.cpp): v' = 1 − v, folded
    // into the decode (q × −scale + 1 − offset). Found by spec 026's probe check on the Old 3DS (the checker's phase).
    const float v_scale = -d.uv_scale[1], v_offset = 1.0f - d.uv_offset[1];
    const float constants[8] = {d.uv_scale[0], v_scale,   d.uv_offset[0], v_offset,
                                m.base[0],     m.base[1], m.base[2],      m.base[3]};
    if (g.cache.constants(constants)) {
        C3D_FVUnifSet(GPU_VERTEX_SHADER, s.uv_decode, d.uv_scale[0], v_scale, d.uv_offset[0], v_offset);
        // The shader multiplies the 0..255 colour attribute by this: base × tint (core folded the tint) / 255.
        C3D_FVUnifSet(GPU_VERTEX_SHADER, s.color_scale, m.base[0] / 255.0f, m.base[1] / 255.0f, m.base[2] / 255.0f,
                      m.base[3] / 255.0f);
    }
    material_state(m);
    C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(d.index_count), C3D_UNSIGNED_SHORT,
                     mesh->indices + d.first_index);
}

void end_3d() {
    if (!ctr::video3d_ready()) return;
    for (int i = 0; i < g.touched_count; ++i) g.touched[i]->param = g.saved[i];
    ctr::lights3d_end();
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_CullFace(GPU_CULL_NONE);
    for (int i = 0; i < 6; ++i) C3D_TexEnvInit(C3D_GetTexEnv(i));
    ctr::restore_2d_state(); // citro2d's shader, attributes, buffers, combiners and blending; depth off
    ctr::context().current_target = nullptr;
    ctr::draw_on(Screen::Top);
}

} // namespace nnh::platform
