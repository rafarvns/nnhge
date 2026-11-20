#pragma once
// What differs between the 3DS and the PC simulator for NNH Link: platform_link_3ds.cpp uses the engine's
// 3DS extension headers (<nnh/3ds/network.hpp>, <nnh/3ds/launch.hpp>), platform_link_pc.cpp has no
// launching. CMake picks one; no #ifdef in the app (constitution I).

#include <cstddef>
#include <cstdint>
#include <string>

namespace link_app::platform {

/// Starts the network (after nnh::init()). @return false if it can't.
bool start_network();
void stop_network();

/// Random bytes for the console id and the connection nonces (the system's generator).
void fill_random(std::uint8_t* out, std::size_t size);

/// Free bytes on the storage holding `folder`, or UINT64_MAX when unknown.
std::uint64_t free_space(const std::string& folder);

/// Starts `path` (a .3dsx) with `args` when Link exits. @return false where that isn't possible.
bool launch_after_exit(const std::string& path, const char* const* args, int count);

} // namespace link_app::platform
