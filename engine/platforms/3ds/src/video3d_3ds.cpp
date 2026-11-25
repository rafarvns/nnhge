// Nintendo 3DS: 3D setup and meshes (contract nnh/platform/video3d.hpp, D43, plan P22). With 3D on, the top screen's
// citro2d target (16-bit depth) is replaced by an RGBA8 + DEPTH24_STENCIL8 target on the same output; the mesh shader
// (shaders/mesh3d.v.pica, embedded by the build) and one attribute layout per vertex format (12/16/20 bytes, missing
// attributes fixed) are set up; 2D keeps drawing with depth off. Meshes live in linear memory.

#include <3ds.h>
#include <citro2d.h>

#include <cstring>
#include <new>

#include "context_3ds.hpp"
#include "nnh_mesh3d_shader_shbin.h"
#include "video3d_3ds.hpp"

namespace nnh::platform {
namespace {

ctr::Shader3D* g_shader = nullptr;

void layout(C3D_AttrInfo& info, std::uint8_t flags) {
    AttrInfo_Init(&info);
    AttrInfo_AddLoader(&info, 0, GPU_SHORT, 4); // position (x, y, z, 0)
    AttrInfo_AddLoader(&info, 1, GPU_BYTE, 4);  // normal
    if ((flags & 1) != 0) AttrInfo_AddLoader(&info, 2, GPU_SHORT, 2); // uv
    if ((flags & 2) != 0) AttrInfo_AddLoader(&info, 3, GPU_UNSIGNED_BYTE, 4); // colour
    if ((flags & 1) == 0) AttrInfo_AddFixed(&info, 2);
    if ((flags & 2) == 0) AttrInfo_AddFixed(&info, 3);
}

u32 transfer_flags() {
    return GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
           GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
           GX_TRANSFER_OUT_FORMAT(static_cast<u32>(gfxGetScreenFormat(GFX_TOP))) |
           GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
}

} // namespace

ctr::Shader3D& ctr::shader3d() { return *g_shader; }
bool ctr::video3d_ready() { return g_shader != nullptr; }

void ctr::restore_2d_state() {
    C2D_Prepare();
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR); // 2D never writes the depth the 3D pass tests
}

bool video3d_init(const Video3DConfig&) {
    g_shader = new (std::nothrow) ctr::Shader3D;
    if (g_shader == nullptr || !ctr::lights3d_init()) {
        ctr::set_error("no memory for 3D");
        return false;
    }
    ctr::Shader3D& s = *g_shader;
    s.dvlb = DVLB_ParseFile(reinterpret_cast<u32*>(const_cast<u8*>(nnh_mesh3d_shader_shbin)),
                            nnh_mesh3d_shader_shbin_size);
    if (s.dvlb == nullptr || shaderProgramInit(&s.program) != 0 ||
        shaderProgramSetVsh(&s.program, &s.dvlb->DVLE[0]) != 0) {
        ctr::set_error("the 3D shader could not be loaded");
        return false;
    }
    s.projection = shaderInstanceGetUniformLocation(s.program.vertexShader, "projection");
    s.model_view = shaderInstanceGetUniformLocation(s.program.vertexShader, "modelView");
    s.normal = shaderInstanceGetUniformLocation(s.program.vertexShader, "normalMtx");
    s.uv_decode = shaderInstanceGetUniformLocation(s.program.vertexShader, "uvDecode");
    s.color_scale = shaderInstanceGetUniformLocation(s.program.vertexShader, "colorScale");
    for (std::uint8_t f = 0; f < 4; ++f) layout(s.attrs[f], f);
    // The top screen's target with a 24-bit depth buffer (citro2d's has 16 bits), on the same output.
    ctr::Context& ctx = ctr::context();
    C3D_RenderTarget* target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (target == nullptr) {
        ctr::set_error("no VRAM for the 3D target");
        return false;
    }
    C3D_RenderTargetDelete(ctx.targets[0]);
    C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, transfer_flags());
    ctx.targets[0] = target;
    ctx.left_target = target;
    // The right eye (spec 026b): the same kind of target on the right output. citro3d copies it to the screen only in
    // frames that drew on it (stereo frames, eyes_3ds.cpp); the others show the left picture to both eyes.
    ctx.right_target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (ctx.right_target == nullptr) {
        ctr::set_error("no VRAM for the right eye's 3D target");
        return false;
    }
    C3D_RenderTargetSetOutput(ctx.right_target, GFX_TOP, GFX_RIGHT, transfer_flags());
    ctx.current_target = nullptr;
    ctr::restore_2d_state();
    return true;
}

void video3d_shutdown() {
    ctr::Context& ctx = ctr::context();
    if (ctx.right_target != nullptr) {
        if (ctx.left_target != nullptr) ctx.targets[0] = ctx.left_target;
        C3D_RenderTargetDelete(ctx.right_target);
        ctx.right_target = ctx.right_eye = nullptr;
        gfxSet3D(false);
    }
    if (g_shader == nullptr) return;
    shaderProgramFree(&g_shader->program);
    if (g_shader->dvlb != nullptr) DVLB_Free(g_shader->dvlb);
    delete g_shader;
    g_shader = nullptr;
}

const char* video3d_self_test() { return ctr::tilt_self_test(); }

bool mesh_create(const MeshUpload& up, MeshInfo& out) {
    out = {};
    const std::size_t vbytes = static_cast<std::size_t>(up.vertex_count) * up.stride, ibytes = up.index_count * 2u;
    auto* m = new (std::nothrow) ctr::Mesh3D;
    void* vertices = linearAlloc(vbytes);
    void* indices = linearAlloc(ibytes);
    if (m == nullptr || vertices == nullptr || indices == nullptr) {
        delete m;
        linearFree(vertices);
        linearFree(indices);
        ctr::set_error("no linear memory for the mesh");
        return false;
    }
    std::memcpy(vertices, up.vertices, vbytes);
    std::memcpy(indices, up.indices, ibytes);
    GSPGPU_FlushDataCache(vertices, vbytes);
    GSPGPU_FlushDataCache(indices, ibytes);
    m->vertices = vertices;
    m->indices = static_cast<std::uint16_t*>(indices);
    m->stride = up.stride;
    m->flags = up.flags;
    m->bytes = vbytes + ibytes;
    out.handle = reinterpret_cast<MeshHandle>(m);
    out.bytes = m->bytes;
    return true;
}

void mesh_destroy(MeshHandle handle) {
    auto* m = reinterpret_cast<ctr::Mesh3D*>(handle);
    if (m == nullptr) return;
    linearFree(m->vertices);
    linearFree(m->indices);
    delete m;
}

} // namespace nnh::platform
