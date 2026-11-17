#pragma once
/// @file nnh/app.hpp
/// Engine lifecycle and the main loop. See docs/api/app.md.

#include <cstdint>

namespace nnh {

/// Options for nnh::init(). Use designated initializers:
/// @code
/// nnh::init({.title = "My Game"});
/// @endcode
struct AppConfig {
    /// Window title in the PC simulator. On consoles the title comes from the game metadata
    /// (TITLE in nnh_add_game()).
    const char* title = "NNHGE";
    /// PC simulator only: initial window size as a multiple of the native resolution (400x480).
    int pc_window_scale = 2;
};

/// Initializes the engine (video, input). Call once, before anything else.
/// @return false if the platform could not be initialized; the game should exit.
bool init(const AppConfig& config = {});

/// Advances one frame: presents what was drawn in the previous frame, waits for vsync (60 Hz),
/// reads input and starts a new frame with the screens cleared.
/// Draw calls are valid between a frame() that returned true and the next frame()/shutdown().
/// @return false when the game must exit (HOME Menu close request, window closed).
/// @code
/// while (nnh::frame()) {
///     if (nnh::input::pressed(nnh::Button::Start)) break;
///     // update and draw
/// }
/// @endcode
bool frame();

/// Presents the last frame if one is open and releases all engine resources.
/// Safe to call more than once.
void shutdown();

/// Number of frames started since init(). The first frame is 1.
std::uint64_t frame_count();

} // namespace nnh
