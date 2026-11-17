#pragma once
/// @file nnh/3ds/console.hpp
/// 3DS-only extension: console details with no portable equivalent. Including this header makes the
/// including file 3DS-only on purpose — keep such code in a few files of your game, behind your own
/// interface. Prefer nnh::system::has(...) for portable checks. See docs/architecture/platforms.md.

#if !defined(NNH_PLATFORM_3DS)
#error "<nnh/3ds/console.hpp> is only available when building for Nintendo 3DS (3ds-* presets)."
#endif

namespace nnh::n3ds {

/// True on New 3DS, New 3DS XL and New 2DS XL.
bool is_new_3ds();

/// Position of the 3D depth slider, 0.0 (off) to 1.0. Always 0.0 on 2DS models.
float slider_3d();

} // namespace nnh::n3ds
