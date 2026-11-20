#pragma once
/// @file nnh/3ds/launch.hpp
/// 3DS-only extension: start another .3dsx when this program exits, through Luma3DS's homebrew loader
/// (`hb:ldr`, the Homebrew Launcher's own mechanism). Including this header makes the including file 3DS-only
/// on purpose. See docs/api/3ds-extensions.md.

#if !defined(NNH_PLATFORM_3DS)
#error "<nnh/3ds/launch.hpp> is only available when building for Nintendo 3DS (3ds-* presets)."
#endif

namespace nnh::n3ds {

/// True when this program was started by Luma3DS's homebrew loader (from the Homebrew Launcher) and the
/// loader answers: only then does launch_after_exit() work. False for a .cia.
bool can_launch();

/// Arranges for `path` ("sdmc:/....3dsx") to start, with argv = {path, args[0..count)}, when this program
/// returns from main (after nnh::shutdown()). The last call wins. An argument "--nnh-return=<path>" makes
/// an NNHGE game start <path> in turn when it exits (NNH Link uses it to come back).
/// @return false if can_launch() is false, the path isn't a .3dsx on the SD card, or the arguments don't
/// fit in 1 KB.
bool launch_after_exit(const char* path, const char* const* args = nullptr, int count = 0);

} // namespace nnh::n3ds
