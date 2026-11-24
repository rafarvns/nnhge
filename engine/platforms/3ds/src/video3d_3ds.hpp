#pragma once
// Nintendo 3DS 3D backend (spec 026): pieces shared by video3d_3ds.cpp (init, meshes), pass3d_3ds.cpp (the pass),
// lights3d_3ds.cpp (light environment and tables) and tilt3d_3ds.cpp (the fixed projection fix-up). Internal.

#include <citro3d.h>

#include "nnh/platform/video3d.hpp"

namespace nnh::platform::ctr {

struct Mesh3D {
    void* vertices = nullptr;      ///< linear memory
    std::uint16_t* indices = nullptr;
    std::uint8_t stride = 12;
    std::uint8_t flags = 0;
    std::size_t bytes = 0;
};

struct Shader3D {
    DVLB_s* dvlb = nullptr;
    shaderProgram_s program{};
    int projection = -1, model_view = -1, normal = -1, uv_decode = -1, color_scale = -1; ///< uniform locations
    C3D_AttrInfo attrs[4];         ///< by mesh flags (uv = 1, colour = 2)
};

/// The 3D state (video3d_3ds.cpp).
Shader3D& shader3d();
bool video3d_ready();

/// Restores the 2D state citro2d expects, then turns depth off for 2D (colour writes only), as every 2D draw of a 3D
/// game needs (plan P22). Called after video3d_init and at the end of each pass.
void restore_2d_state();

/// lights3d_3ds.cpp: tables and light environment.
bool lights3d_init();
/// Sets the pass's ambient and lights (view space).
void lights3d_begin(const Pass3D& pass);
/// Applies a material's lighting (binds or unbinds the environment, specular colour, shininess table).
void lights3d_material(const Material3DGpu& m);
/// Unbinds the environment (fragment lighting off).
void lights3d_end();

/// tilt3d_3ds.cpp: the PICA projection for a core (OpenGL clip space) projection: F × P, F computed once from
/// citro3d's own Mtx_PerspTilt (the depth range and the screen's 90° rotation).
void pica_projection(const float core_projection[16], C3D_Mtx& out);
/// Development: compares F × P with Mtx_PerspTilt and Mtx_OrthoTilt for other parameters.
const char* tilt_self_test();

} // namespace nnh::platform::ctr
