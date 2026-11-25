#pragma once
// PC simulator 3D (spec 026, decision D41): internal pieces shared by video3d_pc.cpp and texels3d_pc.cpp.

#include "nnh/platform/video.hpp"
#include "raster3d.hpp"

namespace nnh::platform::pc {

/// A texture's RGBA pixels for the rasteriser (read back once, freed with the texture). Empty when unknown.
raster3d::TextureView texels3d(TextureHandle handle);

/// eyes_pc.cpp: frees the right eye's target and puts the top screen's own back (video3d_shutdown).
void eyes_shutdown();

} // namespace nnh::platform::pc
