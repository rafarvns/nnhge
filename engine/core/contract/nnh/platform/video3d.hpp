#pragma once
/// @file nnh/platform/video3d.hpp
/// Platform contract — 3D (spec 026, decision D43). Implemented by every platform library, called only by nnh::core's
/// 3D module (so a 2D-only game links none of it). Not for games. See docs/architecture/rendering.md.
///
/// Core does every decision (culling, order, matrices, materials resolved, lights in view space) and calls, per frame
/// and screen with 3D draws: begin_3d → draw_3d × n (opaque and cut-out first, then blended back to front) → end_3d,
/// between the screen's draw_2d calls. Nothing here allocates after video3d_init and mesh_create.

#include <cstddef>
#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {

/// A mesh as the platform knows it (0 = none).
using MeshHandle = std::uintptr_t;

struct Video3DConfig {
    int max_draws = 0; ///< 3D draws per frame (AppConfig::gfx.max_draws_3d), > 0
};

/// Sets up 3D for the top screen: depth targets, shader, lighting tables, per-eye buffers. Called once by core's gfx
/// init when 3D is on. @return false (and last_error()) when it can't (no memory).
bool video3d_init(const Video3DConfig& config);
/// Undoes video3d_init (core destroys every mesh first).
void video3d_shutdown();
/// Debug/Development: the backend's own check of its fixed transforms, as a log line core prints after
/// video3d_init ("3d tilt: PASS ..." / "3d tilt: FAIL ..."), or nullptr when it has none.
const char* video3d_self_test();

/// Quantised vertices and 16-bit indices in the .nnhmesh layout (D42): position int16×4 · normal int8×4 ·
/// [uv int16×2 when flags & 1] · [colour u8×4 when flags & 2]; stride 12, 16 or 20.
struct MeshUpload {
    const void* vertices = nullptr;
    std::uint32_t vertex_count = 0;
    std::uint8_t stride = 12;
    std::uint8_t flags = 0;
    const std::uint16_t* indices = nullptr;
    std::uint32_t index_count = 0;
};
struct MeshInfo {
    MeshHandle handle = 0;
    std::size_t bytes = 0; ///< memory the console uses for it (core accounts it as Linear, tag meshes)
};
/// Copies a mesh's data where the GPU reads it (3DS: linear memory, cache flushed). Load time only (may allocate).
/// @return false (and last_error()) when out of memory.
bool mesh_create(const MeshUpload& upload, MeshInfo& out);
/// Frees a mesh. Core calls it only when no submitted frame uses it.
void mesh_destroy(MeshHandle handle);

inline constexpr int kMaxLights3D = 4;

/// A light in view space. Directional: `vector` = unit direction towards the light. Point: `vector` = position; the
/// falloff is (1 − d/range)², zero beyond `range` (inv_range = 1 / range).
struct Light3DGpu {
    std::uint8_t kind = 0; ///< 0 directional, 1 point
    float vector[3] = {0, 0, 1};
    float color[3] = {1, 1, 1}; ///< diffuse and specular colour, 0..1
    float inv_range = 0;
};

/// One eye's 3D pass on a screen.
struct Pass3D {
    std::uint8_t eye = 0;   ///< 0 left (and mono), 1 right
    float projection[16] = {}; ///< column-major, OpenGL clip space (depth −1..1); the backend maps depth and rotation
    float ambient[3] = {0.3f, 0.3f, 0.3f};
    Light3DGpu lights[kMaxLights3D];
    int light_count = 0;
    bool below_2d = false; ///< 2D was drawn on this screen before the pass (the colour buffer isn't just the clear)
    Color clear{};         ///< the screen's clear colour
};

/// A resolved material (FR-9/FR-10): textures as platform handles, colours 0..1 with the tint folded into base.
struct Material3DGpu {
    TextureHandle texture = 0; ///< 0 = untextured
    float base[4] = {1, 1, 1, 1};
    float specular[3] = {0, 0, 0};
    std::uint8_t shininess = 3;    ///< level 0..7: exponent 2^(level + 1)
    std::uint8_t alpha = 0;        ///< 0 opaque, 1 cut-out, 2 blend
    std::uint8_t cutoff = 128;
    std::uint8_t filter = 0;       ///< 0 nearest, 1 linear
    std::uint8_t wrap_u = 0;       ///< 0 repeat, 1 clamp, 2 mirror
    std::uint8_t wrap_v = 0;
    bool lit = true;
    bool vertex_colors = true;
    bool cull_back = true;
};

/// One draw: a submesh's index range with its model-view (the mesh's quantisation decode folded in, so it applies to
/// the raw int16 positions) and normal matrices, both rows 0..2 (row-major), and the texture coordinate decode.
struct Draw3D {
    MeshHandle mesh = 0;
    std::uint32_t first_index = 0;
    std::uint32_t index_count = 0;
    float model_view[12] = {}; ///< 3 rows × 4: view × model × T(offset) × S(scale)
    float normal[9] = {};      ///< 3 rows × 3: inverse transpose of view × model (3×3), applied to int8 normals
    float uv_scale[2] = {0, 0};
    float uv_offset[2] = {0, 0};
    Material3DGpu material{};
};

/// Starts a 3D pass on `screen` (only Top in spec 026), between its draw_2d calls: depth cleared, lights set.
void begin_3d(Screen screen, const Pass3D& pass);
/// Draws one submesh range. Must not allocate.
void draw_3d(const Draw3D& draw);
/// Ends the pass and restores the 2D state (draw_2d works again).
void end_3d();

} // namespace nnh::platform
