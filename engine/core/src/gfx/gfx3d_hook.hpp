#pragma once
// The 3D hook (spec 026, plan P14, D43): 2D graphics reach the 3D module only through this one pointer, set by a
// registrar in gfx3d/gfx3d_ops.cpp. That object is linked only when a game uses meshes or 3D drawing, so a 2D game
// links no 3D code; for it the pointer stays null and every call site is one null check. Internal.

#include "nnh/app.hpp"
#include "nnh/gfx.hpp"

namespace nnh::detail {

struct Gfx3DOps {
    /// After the 2D state is ready, when AppConfig::gfx.max_draws_3d > 0. @return false to fail nnh::init().
    bool (*init)(const GfxConfig& config) = nullptr;
    /// First thing in gfx_shutdown(): meshes destroyed, platform 3D shut down, memory freed.
    void (*shutdown)() = nullptr;
    /// Submits `screen`'s 2D pass with the 3D pass inside it. @return false when the screen has no 3D this frame
    /// (core then submits the 2D list exactly as without 3D).
    bool (*submit_screen)(Screen screen) = nullptr;
    /// After the frame was submitted: retires the meshes released until now, clears the frame's records.
    void (*after_frame)() = nullptr;
    /// After the platform waited for the GPU (D47): frees the retired meshes.
    void (*gpu_idle)() = nullptr;
    /// Writes the debug overlay's 3D line (FR-28) into `out`. @return false when there is nothing to show.
    bool (*overlay_line)(char* out, int size) = nullptr;
};

/// Null unless the game links the 3D module (gfx3d_hook.cpp).
extern const Gfx3DOps* g_gfx_3d_ops;

} // namespace nnh::detail
