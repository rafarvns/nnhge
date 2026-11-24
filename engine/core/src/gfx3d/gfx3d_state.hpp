#pragma once
// The 3D module's state (spec 026): capacities, the frame's draw records, cameras, lights, stats. Internal; one
// instance, single-threaded. Reserved once at init when AppConfig::gfx.max_draws_3d > 0 (FR-26).

#include <cstdint>

#include "nnh/app.hpp"
#include "nnh/gfx.hpp"
#include "nnh/gfx3d.hpp"

namespace nnh::detail {

inline constexpr std::uint8_t kRecordHasMaterial = 1;

/// One recorded draw (88 bytes on 32-bit systems): the model's rows 0..2, an optional material, the mesh by handle.
struct DrawRecord {
    float model[12];             ///< rows 0..2 of the model matrix (row-major)
    Material3D material;         ///< the override when flags & kRecordHasMaterial
    std::uint32_t mesh_generation;
    std::int16_t mesh_slot;
    std::int8_t submesh;         ///< −1 = all
    std::uint8_t flags;
    Color tint;
    std::uint16_t call;          ///< call index this frame
    std::uint16_t reserved;
};

struct State3D {
    bool ready = false;
    int max_draws = 0;
    DrawRecord* records = nullptr;
    Sphere* spheres = nullptr;       ///< world bounding sphere per record (hot array for culling)
    std::uint16_t* order = nullptr;  ///< the frame's submission order
    std::uint16_t* scratch = nullptr;
    float* keys = nullptr;           ///< view depth per record (blended sort)
    bool* visible = nullptr;         ///< per record, this frame: inside the view
    bool* blended = nullptr;         ///< per record: draws a blended part
    int count = 0;
    int dropped = 0;
    int dropped_total = 0;
    int peak = 0;
    int frame_culled = 0;            ///< this frame so far (copied to `last` when it ends)
    int frame_triangles[2] = {0, 0};
    int frame_eyes = 0;
    Camera3D cameras[2];
    Lighting lighting[2];
    int world_layer[2] = {8, 8};
    Stats3D last{};
    bool debug_draw = false;
};

State3D& state3d();

/// gfx3d_init.cpp: reserves everything (records, spheres, order, sort scratch), the mesh table and the platform's 3D.
bool gfx3d_init(const GfxConfig& config);
void gfx3d_shutdown();
/// True between gfx3d_init and gfx3d_shutdown.
bool gfx3d_ready();
/// screen_pass.cpp: the screen's 2D pass with the 3D pass inside. False when it has no 3D draws this frame.
bool gfx3d_submit_screen(Screen screen);
/// After the frame: stats kept, the records cleared for the next one.
void gfx3d_frame_done();
/// stats3d.cpp: the overlay's 3D line.
bool gfx3d_overlay_line(char* out, int size);
/// debug_draw3d.cpp: records the debug lines of `screen`'s draws (Debug/Development, when on).
void gfx3d_debug_lines(Screen screen);

/// camera3d.cpp: a camera's view matrix (world → camera, rigid) and projection (OpenGL clip space, 400×240 aspect).
Mat4 camera_view(const Camera3D& camera);
Mat4 camera_projection(const Camera3D& camera);
/// A valid camera: finite, planes and field of view in range, a usable rotation.
bool camera_valid(const Camera3D& camera);

/// material_check.cpp: false when `m` has an out-of-range value; then `m` is clamped to a valid one.
bool check_material(Material3D& m);

} // namespace nnh::detail
