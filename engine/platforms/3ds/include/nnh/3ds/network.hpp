#pragma once
/// @file nnh/3ds/network.hpp
/// 3DS-only extension: start the console's socket service, so the game can use BSD sockets (<sys/socket.h>).
/// Including this header makes the including file 3DS-only on purpose — keep such code in a few files of
/// your game, behind your own interface. See docs/api/3ds-extensions.md.

#if !defined(NNH_PLATFORM_3DS)
#error "<nnh/3ds/network.hpp> is only available when building for Nintendo 3DS (3ds-* presets)."
#endif

namespace nnh::n3ds {

/// Starts the socket service (libctru's soc, with a 1 MB buffer from the heap). Reference-counted and shared
/// with the engine's log redirect of Development builds: each call that returns true needs one
/// network_stop(). Call it after nnh::init(). It doesn't wait for Wi-Fi: with Wi-Fi off, sockets fail.
/// @return false if the memory or the service is unavailable.
bool network_start();

/// Releases one network_start(). The service stops when the last user stops; nnh::shutdown() releases
/// whatever the game didn't.
void network_stop();

} // namespace nnh::n3ds
