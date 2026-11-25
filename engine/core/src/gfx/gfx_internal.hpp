#pragma once
// Graphics hooks for the frame loop and other engine modules. Internal.

#include "nnh/app.hpp"
#include "nnh/gfx.hpp"

namespace nnh::detail {

/// Clear color set by nnh::gfx::set_clear_color() for a screen.
Color clear_color(Screen screen);

/// Allocates the draw list and texture table (AppConfig::gfx) and resets cameras. @return false if
/// out of memory.
bool gfx_init(const GfxConfig& config);

/// Destroys every texture still loaded (their handles go stale) and frees the lists.
void gfx_shutdown();

/// End of frame, before the debug pass: submits the 2D pass of each screen, then clears the list.
void gfx_submit();

/// Right after platform::begin_frame() (the GPU finished the previous frame, D47): frees the textures and meshes
/// retired by gfx_frame_submitted() before it.
void gfx_gpu_idle();

/// After platform::end_frame(): what was released until now is retired (the GPU may draw it in the frame just
/// submitted) and freed by the next gfx_gpu_idle(); the 3D module's per-frame records are cleared.
void gfx_frame_submitted();

} // namespace nnh::detail
