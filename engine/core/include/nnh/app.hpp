#pragma once
/// @file nnh/app.hpp
/// Engine lifecycle and the main loop. See docs/api/app.md.

#include <cstddef>
#include <cstdint>

namespace nnh {

/// Event bus capacities (see <nnh/events.hpp>), allocated once at init().
struct EventsConfig {
    /// Ring buffer for queued events. Each event takes 16 bytes + its size rounded up to 8.
    /// Minimum 1024.
    std::size_t queue_bytes = 16 * 1024;
    /// Maximum simultaneous subscriptions (about 48 bytes each on 3DS). Minimum 8.
    int max_subscribers = 256;
};

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
    /// Name of the game's data folder (saves, log.txt, crash.txt): letters, digits, '-' and '_'.
    /// 3DS: sdmc:/3ds/<data_folder>/. nullptr = derived from `title` ("My Game" -> "my-game").
    const char* data_folder = nullptr;
    /// Event bus capacities: `.events = {.queue_bytes = 32 * 1024, .max_subscribers = 512}`.
    EventsConfig events = {};
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
