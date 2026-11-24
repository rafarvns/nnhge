#pragma once
/// @file nnh/app.hpp
/// Engine lifecycle and the main loop. See docs/api/app.md.

#include <cstddef>
#include <cstdint>

// The game's TITLE from nnh_add_game() (spec 009): build metadata, not a platform switch.
#ifndef NNH_GAME_TITLE
#define NNH_GAME_TITLE "NNHGE"
#endif

namespace nnh {

/// Event bus capacities (see <nnh/events.hpp>), allocated once at init().
struct EventsConfig {
    /// Ring buffer for queued events. Each event takes 16 bytes + its size rounded up to 8.
    /// Minimum 1024.
    std::size_t queue_bytes = 16 * 1024;
    /// Maximum simultaneous subscriptions (about 48 bytes each on 3DS). Minimum 8.
    int max_subscribers = 256;
};

/// 2D graphics capacities (see <nnh/gfx.hpp>, <nnh/texture.hpp>), allocated once at init().
struct GfxConfig {
    /// 2D draws (sprites and shapes, both screens) per frame; more are dropped. About 136 bytes each.
    /// 16 to 65535.
    int max_draws = 2048;
    /// Textures loaded at the same time. About 32 bytes each. Minimum 4.
    int max_textures = 128;
    /// Tile quads per frame (both screens) drawn by gfx::draw_tilemap (<nnh/tilemap.hpp>); more are dropped.
    /// 0 (default) = no tilemap drawing and no memory for it. Up to 65535. Costs ~20 KB once when > 0, and on
    /// 3DS one citro2d quad (~192 bytes, Linear) each: a full 16x16 layer of the top screen is up to 416
    /// tiles. An Old 3DS draws 4 such layers + 300 sprites at 60 Hz (limits: 6 layers, or 600 sprites).
    int max_tiles = 0;
    /// Particle quads per frame (both screens) drawn by ParticleEmitter::draw (<nnh/particles.hpp>); more are dropped.
    /// 0 (default) = no particle drawing and no memory for it. Up to 65535. Costs ~8 KB once when > 0, and on 3DS one
    /// citro2d quad (~192 bytes, Linear) each.
    int max_particles = 0;
};

/// Audio capacities (see <nnh/audio.hpp>), allocated once at init().
struct AudioConfig {
    /// Sounds that can play at the same time, 1 to 22 (the 3DS DSP has 24 channels; music uses 2).
    int voices = 16;
    /// Sounds loaded at the same time. About 40 bytes each. Minimum 1.
    int max_sounds = 128;
};

/// Options for nnh::init(). Use designated initializers:
/// @code
/// nnh::init({.title = "My Game"});
/// @endcode
struct AppConfig {
    /// Window title in the PC simulator. On consoles the title comes from the game metadata
    /// (TITLE in nnh_add_game()). Defaults to that TITLE (the build defines NNH_GAME_TITLE for each
    /// game), so a game rarely needs to set it.
    const char* title = NNH_GAME_TITLE;
    /// PC simulator only: initial window size as a multiple of the native resolution (400x480).
    int pc_window_scale = 2;
    /// Name of the game's data folder (saves, log.txt, crash.txt): letters, digits, '-' and '_'.
    /// 3DS: sdmc:/3ds/<data_folder>/. nullptr = derived from `title` ("My Game" -> "my-game").
    const char* data_folder = nullptr;
    /// Event bus capacities: `.events = {.queue_bytes = 32 * 1024, .max_subscribers = 512}`.
    EventsConfig events = {};
    /// 2D graphics capacities: `.gfx = {.max_draws = 4096}`.
    GfxConfig gfx = {};
    /// Audio capacities: `.audio = {.voices = 8}`.
    AudioConfig audio = {};
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
