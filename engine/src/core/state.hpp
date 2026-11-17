#pragma once
// Engine-wide state shared between core modules. Internal.

namespace nnh::detail {

/// True between a nnh::frame() that returned true and the next frame()/shutdown().
bool frame_open();

} // namespace nnh::detail
