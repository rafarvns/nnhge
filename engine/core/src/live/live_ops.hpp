#pragma once
// The live module's ops table (debug/live_hook.hpp): published by the first registration, withdrawn with the registry.
// Also the frame end (follows, dumps, the per-frame watches' sweep) and the Stats page's line.

#include <cstdint>

namespace nnh::detail::live {

void install_ops();
void uninstall_ops();

/// The frame end for `frame` (paused: a paused tick, frame = the last game frame).
void frame_end(std::uint32_t frame, bool paused);

/// `live 12/128 tun 5/64 wat 3/64 shown 20.1 KB` (+ ` refused N`); false without a registry.
bool stats_line(char* out, int size);

} // namespace nnh::detail::live
