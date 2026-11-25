#pragma once
// Engine-wide frame state shared by modules. Internal; owned by app/app.cpp.

namespace nnh::detail {

/// True between a nnh::frame() that returned true and the next frame()/shutdown().
bool frame_open();

/// True while a frame is open or the GPU may still draw a submitted one (decision D47: the platform waits for it only
/// in the next frame's begin_frame()): a texture or mesh released now is freed after that wait, never at once.
bool gpu_may_use_resources();

/// True between a successful nnh::init() and nnh::shutdown().
bool engine_initialized();

} // namespace nnh::detail
