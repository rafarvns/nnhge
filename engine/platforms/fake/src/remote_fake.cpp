// Fake platform: the console remote (contract nnh/platform/remote.hpp). An in-memory "PC": tests push the
// bytes it sends, read back what core sent, and get synthetic screen captures (nnh/platform_fake.hpp).
// Test-only code: std::vector is fine here (the engine's no-heap rule is about nnh::core).

#include <cstring>
#include <vector>

#include "fake_internal.hpp"
#include "nnh/platform/remote.hpp"

namespace nnh {
namespace {

struct Remote {
    bool connected = false;
    bool closed = false; // the PC hung up: -1 once the pushed bytes are read
    std::vector<std::uint8_t> inbox;
    std::size_t read = 0;
    std::vector<std::uint8_t> sent;
    bool capture_fails = false;
};

Remote g_remote;
std::uint8_t g_capture[400 * 240 * 4]; // the largest screen, RGBA8

} // namespace

void platform_fake::detail::remote_reset() { g_remote = Remote{}; }

void platform_fake::remote_connect() {
    g_remote.connected = true;
    g_remote.closed = false;
}

void platform_fake::remote_push(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    g_remote.inbox.insert(g_remote.inbox.end(), bytes, bytes + size);
}

void platform_fake::remote_disconnect() { g_remote.closed = true; }

const std::uint8_t* platform_fake::remote_sent(std::size_t& size) {
    size = g_remote.sent.size();
    return g_remote.sent.data();
}

void platform_fake::remote_clear_sent() { g_remote.sent.clear(); }

void platform_fake::set_remote_capture_fails(bool fails) { g_remote.capture_fails = fails; }

void platform_fake::remote_capture_pixel(Screen screen, int x, int y, std::uint8_t rgba[4]) {
    rgba[0] = static_cast<std::uint8_t>(x);
    rgba[1] = static_cast<std::uint8_t>(y);
    rgba[2] = screen == Screen::Top ? 0x40 : 0x80;
    rgba[3] = 0xFF;
}

long platform::remote_receive(void* buffer, std::size_t size) {
    platform_fake::State& s = platform_fake::detail::mutable_state();
    ++s.remote_receive_calls;
    s.remote_last_read = size;
    const std::size_t pending = g_remote.inbox.size() - g_remote.read;
    if (!g_remote.connected || (g_remote.closed && pending == 0)) return -1;
    const std::size_t n = pending < size ? pending : size;
    if (n > 0) std::memcpy(buffer, g_remote.inbox.data() + g_remote.read, n);
    g_remote.read += n;
    return static_cast<long>(n);
}

bool platform::remote_send(const void* data, std::size_t size) {
    if (!g_remote.connected || g_remote.closed) return false;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    g_remote.sent.insert(g_remote.sent.end(), bytes, bytes + size);
    return true;
}

bool platform::remote_capture(Screen screen, ScreenPixels& out) {
    platform_fake::State& s = platform_fake::detail::mutable_state();
    if (g_remote.capture_fails) {
        platform_fake::detail::set_last_error("fake capture failure (set_remote_capture_fails)");
        return false;
    }
    if (s.frame_open) { // the contract reads what the frame that just ended drew
        platform_fake::detail::set_last_error("remote_capture() while a frame is being drawn");
        return false;
    }
    const int width = screen_width(screen);
    const int height = screen_height(screen);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) platform_fake::remote_capture_pixel(screen, x, y, g_capture + (y * width + x) * 4);
    }
    out.data = g_capture;
    out.size = static_cast<std::uint32_t>(width * height * 4);
    out.width = static_cast<std::uint16_t>(width);
    out.height = static_cast<std::uint16_t>(height);
    out.layout = PixelLayout::Rgba8Rows;
    ++s.remote_captures;
    s.remote_capture_held = true;
    return true;
}

void platform_fake::remote_capture_right_pixel(int x, int y, std::uint8_t rgba[4]) {
    remote_capture_pixel(Screen::Top, x, y, rgba);
    rgba[2] = 0xC0;
}

bool platform::remote_capture_right(ScreenPixels& out) {
    if (!platform_fake::pass3d_log().last_frame_stereo) return remote_capture(Screen::Top, out); // one picture for both eyes
    if (!remote_capture(Screen::Top, out)) return false;
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 400; ++x) platform_fake::remote_capture_right_pixel(x, y, g_capture + (y * 400 + x) * 4);
    }
    return true;
}

void platform::remote_capture_release() {
    platform_fake::State& s = platform_fake::detail::mutable_state();
    if (s.remote_capture_held) ++s.remote_capture_releases;
    s.remote_capture_held = false;
}

} // namespace nnh
