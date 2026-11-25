// Probe pixels (nnh/gfx3d.hpp request_probes, FR-24, plan P19): a game's check mode asks for a few pixels of the frame
// being drawn; after that frame ends they are read back through the remote capture contract (the screenshot path of
// spec 013: SDL_RenderReadPixels on PC, the framebuffer on 3DS) and answered from the next frame. Debug/Development
// only: Release compiles it to nothing (no remote capture there).

#include "gfx3d/gfx3d_state.hpp"
#include "nnh/build.hpp"
#include "log/engine_channels.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/remote.hpp"

namespace nnh {
namespace detail {
namespace {

constexpr int kMaxProbes = 32;
/// Frame ends to wait before reading, so the picture has settled (the game keeps drawing the same scene until
/// probe_results() answers).
constexpr int kCaptureAfterFrames = 3;

struct Probes {
    Probe points[kMaxProbes];
    int requested = 0; ///< pending
    int wait = 0;      ///< frame ends left before the read
    Color results[kMaxProbes];
    int answered = 0;
};
Probes g_probes;

Color read(const platform::ScreenPixels& s, int x, int y) {
    if (x < 0 || y < 0 || x >= s.width || y >= s.height) return {};
    if (s.layout == platform::PixelLayout::Rgba8Rows) {
        const std::uint8_t* p = s.data + (static_cast<std::size_t>(y) * s.width + x) * 4;
        return {p[0], p[1], p[2], 255};
    }
    const std::uint8_t* p = s.data + (static_cast<std::size_t>(x) * s.height + (s.height - 1 - y)) * 3; // B, G, R
    return {p[2], p[1], p[0], 255};
}

} // namespace

void gfx3d_probes_after_frame() {
    if constexpr (build::kDevToolsEnabled) {
        if (g_probes.requested == 0 || --g_probes.wait > 0) return;
        platform::ScreenPixels px;
        const int n = g_probes.requested;
        g_probes.requested = 0;
        if (!platform::remote_capture(Screen::Top, px)) {
            NNH_LOG_WARN(gfx, "3d probes: the screen read-back failed: %s", platform::last_error());
            return;
        }
        for (int i = 0; i < n; ++i) g_probes.results[i] = read(px, g_probes.points[i].x, g_probes.points[i].y);
        g_probes.answered = n;
    }
}

} // namespace detail

bool gfx3d::request_probes(Screen screen, Eye eye, const Probe* points, int count) {
    if constexpr (!build::kDevToolsEnabled) {
        (void)screen, (void)eye, (void)points, (void)count;
        return false;
    } else {
        if (screen != Screen::Top || eye != Eye::Left || points == nullptr || count < 1 || count > detail::kMaxProbes)
            return false;
        for (int i = 0; i < count; ++i) detail::g_probes.points[i] = points[i];
        detail::g_probes.requested = count;
        detail::g_probes.wait = detail::kCaptureAfterFrames;
        detail::g_probes.answered = 0;
        return true;
    }
}

int gfx3d::probe_results(Color* out, int capacity) {
    const int n = detail::g_probes.answered < capacity ? detail::g_probes.answered : capacity;
    for (int i = 0; i < n; ++i) out[i] = detail::g_probes.results[i];
    return n;
}

} // namespace nnh
