#pragma once
// Nintendo 3DS: what a TextureHandle points to, for texture_alpha_3ds.cpp (spec 029). texture_3ds.cpp keeps its own
// copy of this struct in its anonymous namespace — the same members in the same order — so that file, and with it the
// debug information of every game's .elf (hello's included), stays byte for byte as it was; texture_destroy() frees
// handles of both files. Internal to the 3DS platform.

#include <3ds.h>
#include <citro2d.h>
#include <tex3ds.h>

namespace nnh::platform::ctr {

/// A citro3d texture in linear memory, and the subtexture table tex3ds allocated (nullptr for RGBA and coverage
/// textures). Layout shared with texture_3ds.cpp's Texture3ds.
struct Texture3ds {
    C3D_Tex tex;
    Tex3DS_Texture t3x;
};
static_assert(sizeof(Texture3ds) == sizeof(C3D_Tex) + sizeof(void*) + (sizeof(C3D_Tex) % alignof(void*)),
              "texture_3ds.cpp's layout: tex then t3x");

} // namespace nnh::platform::ctr
