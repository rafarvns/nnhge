#pragma once
// Engine-wide frame state shared by modules. Internal; owned by app/app.cpp.

namespace nnh::detail {

/// True between a nnh::frame() that returned true and the next frame()/shutdown().
bool frame_open();

} // namespace nnh::detail
