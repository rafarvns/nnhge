#pragma once
// Graphics state other engine modules need. Internal.

#include "nnh/gfx.hpp"

namespace nnh::detail {

/// Clear color set by nnh::gfx::set_clear_color() for a screen.
Color clear_color(Screen screen);

} // namespace nnh::detail
