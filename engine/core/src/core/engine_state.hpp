#pragma once
// Engine-wide frame state shared by modules. Internal; owned by app/app.cpp.

namespace nnh::detail {

/// True between a nnh::frame() that returned true and the next frame()/shutdown().
bool frame_open();

/// True between a successful nnh::init() and nnh::shutdown().
bool engine_initialized();

} // namespace nnh::detail
