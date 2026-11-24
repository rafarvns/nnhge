#pragma once
// The 3D module's state (spec 026): capacities, the frame's draw records, cameras, lights, stats. Internal; one
// instance, single-threaded. Reserved once at init when AppConfig::gfx.max_draws_3d > 0 (FR-26).

#include <cstdint>

#include "nnh/app.hpp"
#include "nnh/gfx.hpp"

namespace nnh::detail {

/// gfx3d_init.cpp: reserves everything (records, spheres, order, sort scratch), the mesh table and the platform's 3D.
bool gfx3d_init(const GfxConfig& config);
void gfx3d_shutdown();
/// True between gfx3d_init and gfx3d_shutdown.
bool gfx3d_ready();
/// screen_pass.cpp: the screen's 2D pass with the 3D pass inside. False when it has no 3D draws this frame.
bool gfx3d_submit_screen(Screen screen);
/// After the frame: the records are cleared for the next one.
void gfx3d_frame_done();
/// stats3d.cpp: the overlay's 3D line.
bool gfx3d_overlay_line(char* out, int size);

} // namespace nnh::detail
