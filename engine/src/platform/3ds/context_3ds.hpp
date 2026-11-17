#pragma once
// State shared by the 3DS backend files. Internal to engine/src/platform/3ds/.

#include <citro2d.h>

#include "nnh/gfx.hpp"

namespace nnh::platform::ctr {

struct Context {
    C3D_RenderTarget* targets[2] = {nullptr, nullptr}; // [0] top, [1] bottom
    C3D_RenderTarget* current_target = nullptr;        // scene currently begun this frame
    C2D_TextBuf text_buffer = nullptr;                 // debug text glyphs, cleared every frame
};

Context& context();

inline u32 to_c2d(Color c) { return C2D_Color32(c.r, c.g, c.b, c.a); }

/// Makes `screen` the target of the following citro2d draw calls (no-op if already current).
void draw_on(Screen screen);

} // namespace nnh::platform::ctr
