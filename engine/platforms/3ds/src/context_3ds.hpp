#pragma once
// State shared by the 3DS platform library files. Internal to engine/platforms/3ds/.

#include <citro2d.h>

#include <cstdint>

#include "nnh/gfx.hpp"

namespace nnh::platform::ctr {

struct Context {
    C3D_RenderTarget* targets[2] = {nullptr, nullptr}; // [0] top, [1] bottom
    C3D_RenderTarget* current_target = nullptr;        // scene currently begun this frame
    bool c_stick_ready = false;                        // irrst started (New 3DS)
};

Context& context();

/// The citro3d texture behind a platform::TextureHandle (texture_3ds.cpp).
C3D_Tex* texture_of(std::uintptr_t handle);

/// Sets platform::last_error() (a string literal: it is kept, not copied).
void set_error(const char* what);

inline u32 to_c2d(Color c) { return C2D_Color32(c.r, c.g, c.b, c.a); }

/// Starts the C-Stick service on New 3DS models (input_3ds.cpp; called by init). Without it the C-Stick
/// reads 0.
void input_start();
void input_stop();

/// Makes `screen` the target of the following citro2d draw calls (no-op if already current).
void draw_on(Screen screen);

} // namespace nnh::platform::ctr
