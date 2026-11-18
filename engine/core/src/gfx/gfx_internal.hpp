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

/// After platform::end_frame(): frees textures released during the frame.
void gfx_after_frame();

} // namespace nnh::detail
