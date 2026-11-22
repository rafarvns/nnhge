#pragma once
/// @file nnh/platform/remote.hpp
/// Platform contract — the console remote (spec 013, D28): bytes to and from the PC that started the game
/// (`nnh run`), and reading the screens back for screenshots. Implemented by every platform library, called
/// only by nnh::core, and only in Debug/Development builds (core never calls it in Release, so the linker
/// leaves these functions out). Not for games. See specs/013-console-remote/plan.md.
///
/// Implementations: pc `remote_pc.cpp` (a loopback socket to the `nnh run --pc` named by NNH_REMOTE; the log goes
/// through it too) and `remote_capture_pc.cpp` (SDL_RenderReadPixels, layout Rgba8Rows); 3ds `remote_3ds.cpp` (the
/// 3dslink log socket) and `remote_capture_3ds.cpp` (C3D_SyncDisplayTransfer into 288 000 bytes of linear memory,
/// layout Bgr8ColumnsBottomUp; a screenshot pair costs 0.6–0.8 s over Wi-Fi on an Old 3DS); fake `remote_fake.cpp`
/// (scriptable bytes, recorded sends, a synthetic pattern). Conformance: tests/platform_conformance/. Users' guide:
/// docs/guides/testing-on-the-console.md.

#include <cstddef>
#include <cstdint>

#include "nnh/gfx.hpp"

namespace nnh::platform {

/// Reads bytes the PC sent, never waiting. Called at most once per frame, with `size` bounded by core.
/// @return > 0 bytes read, 0 nothing yet, -1 no connection (never had one, or it closed). After -1 core
/// stops calling. Must not allocate.
long remote_receive(void* buffer, std::size_t size);

/// Sends bytes to the PC after everything the program wrote to its log before (3DS: flushes stdout/stderr
/// first), so records and log lines arrive in order. May block until sent (ticks are a few bytes; screenshots
/// are the only large sends). @return false if there is no connection.
bool remote_send(const void* data, std::size_t size);

/// How a screen's pixels are laid out in ScreenPixels::data.
enum class PixelLayout : std::uint8_t {
    Rgba8Rows = 1,           ///< R, G, B, A per pixel, rows top-down (PC)
    Bgr8ColumnsBottomUp = 2, ///< B, G, R per pixel, columns left to right, each bottom to top (3DS framebuffer)
};

/// One screen read back. `data` belongs to the platform.
struct ScreenPixels {
    const std::uint8_t* data = nullptr;
    std::uint32_t size = 0; ///< bytes: width × height × 4 (Rgba8Rows) or × 3 (Bgr8ColumnsBottomUp)
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    PixelLayout layout = PixelLayout::Rgba8Rows;
};

/// Reads `screen` back as the frame that just ended drew it (core calls it right after end_frame()). The
/// first call may allocate the read-back buffer (3DS: linear memory); `out` stays valid until the next call
/// or remote_capture_release(). @return false (and last_error()) if it can't.
bool remote_capture(Screen screen, ScreenPixels& out);

/// Frees the read-back buffer, if any (end of a remote session, shutdown). Safe to call at any time.
void remote_capture_release();

} // namespace nnh::platform
