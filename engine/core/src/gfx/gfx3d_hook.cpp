// The 3D hook's pointer (gfx3d_hook.hpp): constant-initialised to null, set by gfx3d/gfx3d_ops.cpp's registrar.

#include "gfx/gfx3d_hook.hpp"

namespace nnh::detail {

const Gfx3DOps* g_gfx_3d_ops = nullptr;

} // namespace nnh::detail
