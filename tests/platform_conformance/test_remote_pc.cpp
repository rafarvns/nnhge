// PC platform only: the console remote's transport and read-back (spec 013 T4.1, T4.2). The test plays nnh:
// it listens on the loopback interface, points NNH_REMOTE at itself and checks the bytes both ways, the log
// going through the socket while connected, and the screens read back as drawn. Linked into
// nnh_conformance_pc (headless: offscreen video, software renderer).

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "nnh/build.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/log.hpp"
#include "nnh/platform/remote.hpp"
#include "nnh/platform/video.hpp"
#include "nnh_link/net.hpp"
#include "test.hpp"

#ifdef _WIN32
#include <io.h> // _dup/_dup2: the test captures stdout
#else
#include <unistd.h>
#endif

namespace {

/// Points stdout at `file` until restore() (file descriptors, so printf from any code lands there).
struct StdoutCapture {
    int saved = -1;
    explicit StdoutCapture(std::FILE* file) {
        std::fflush(stdout);
#ifdef _WIN32
        saved = _dup(_fileno(stdout));
        _dup2(_fileno(file), _fileno(stdout));
#else
        saved = dup(fileno(stdout));
        dup2(fileno(file), fileno(stdout));
#endif
    }
    void restore() {
        std::fflush(stdout);
#ifdef _WIN32
        _dup2(saved, _fileno(stdout));
        _close(saved);
#else
        dup2(saved, fileno(stdout));
        ::close(saved);
#endif
    }
};

} // namespace

namespace platform = nnh::platform;
namespace net = nnh::link::net;

namespace {

const nnh::link::Ipv4 kLoopback{{127, 0, 0, 1}};

void set_remote(const char* value) {
#ifdef _WIN32
    _putenv_s("NNH_REMOTE", value != nullptr ? value : "");
#else
    if (value != nullptr) setenv("NNH_REMOTE", value, 1);
    else unsetenv("NNH_REMOTE");
#endif
}

nnh::AppConfig config() {
    nnh::AppConfig c;
    c.title = "conformance";
    c.pc_window_scale = 1;
    return c;
}

/// Calls remote_receive until it returns something other than 0 (bytes arrive over a real socket).
long receive_soon(void* buffer, std::size_t size) {
    for (int i = 0; i < 200; ++i) {
        const long n = platform::remote_receive(buffer, size);
        if (n != 0) return n;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

bool pixel_is(const platform::ScreenPixels& px, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    const std::uint8_t* p = px.data + (static_cast<std::size_t>(y) * px.width + x) * 4;
    return p[0] == r && p[1] == g && p[2] == b && p[3] == 255;
}

} // namespace

NNH_TEST(pc_remote_ignores_an_address_off_this_computer) {
    set_remote("192.168.10.52:17491");
    CHECK(platform::init(config()));
    unsigned char buffer[16] = {};
    CHECK(platform::remote_receive(buffer, sizeof buffer) == -1);
    CHECK(!platform::remote_send(buffer, sizeof buffer));
    platform::shutdown();
    set_remote(nullptr);
}

NNH_TEST(pc_remote_connects_back_to_nnh_and_moves_bytes_both_ways) {
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    CHECK(listener.valid());
    const std::string address = "127.0.0.1:" + std::to_string(listener.local_port());
    set_remote(address.c_str());
    CHECK(platform::init(config()));
    set_remote(nullptr);
    net::Socket nnh = net::tcp_accept(listener, nnh::build::kDevToolsEnabled ? 2000 : 200);
    unsigned char got[32] = {};
    if constexpr (!nnh::build::kDevToolsEnabled) {
        CHECK(!nnh.valid()); // Release: no remote at all
        CHECK(platform::remote_receive(got, sizeof got) == -1);
        platform::shutdown();
        return;
    }
    CHECK(nnh.valid());
    CHECK(platform::remote_receive(got, sizeof got) == 0); // connected, nothing sent yet: never waits
    const unsigned char message[16] = {'R', 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    CHECK(nnh.send_all(message, sizeof message));
    CHECK(receive_soon(got, 8) == 8); // bounded by the size asked for
    CHECK(receive_soon(got + 8, sizeof got - 8) == 8);
    CHECK(std::memcmp(got, message, sizeof message) == 0);

    CHECK(platform::remote_send("\x1ENR", 3));
    platform::log_console(nnh::LogLevel::Warn, "[f1        0.016s] W test   through nnh");
    const char expected[] = "\x1ENR[f1        0.016s] W test   through nnh\n";
    char text[sizeof expected] = {};
    nnh.set_recv_timeout(2000);
    CHECK(nnh.recv_all(text, sizeof expected - 1));
    CHECK(std::strcmp(text, expected) == 0); // in order, uncoloured, one line

    nnh.close();
    CHECK(receive_soon(got, sizeof got) == -1);
    CHECK(!platform::remote_send(got, 1));
    platform::log_console(nnh::LogLevel::Info, "[f2        0.033s] I test   back on stdout"); // must not fail
    platform::shutdown();
}

NNH_TEST(pc_remote_sends_a_screenshot_sized_block_whole_and_logs_only_to_nnh) {
    if constexpr (!nnh::build::kDevToolsEnabled) return; // Release never connects
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    const std::string address = "127.0.0.1:" + std::to_string(listener.local_port());
    set_remote(address.c_str());
    CHECK(platform::init(config()));
    set_remote(nullptr);
    net::Socket nnh = net::tcp_accept(listener, 2000);
    CHECK(nnh.valid());
    // A screenshot pair is ~520 KB: far more than a socket buffer, so the send must wait for nnh to read.
    static unsigned char block[1 << 20];
    for (std::size_t i = 0; i < sizeof block; ++i) block[i] = static_cast<unsigned char>(i * 7);
    static unsigned char got[sizeof block];
    bool received = false;
    std::thread reader([&] {
        nnh.set_recv_timeout(5000);
        received = nnh.recv_all(got, sizeof got);
    });
    CHECK(platform::remote_send(block, sizeof block));
    reader.join();
    CHECK(received && std::memcmp(got, block, sizeof block) == 0);

    // While connected a line goes to nnh and not to the terminal (nnh shows it: never twice).
    std::FILE* capture = std::tmpfile();
    StdoutCapture to_file(capture);
    platform::log_console(nnh::LogLevel::Error, "[f3        0.050s] E test   only to nnh");
    to_file.restore();
    CHECK(std::fseek(capture, 0, SEEK_END) == 0 && std::ftell(capture) == 0);
    std::fclose(capture);
    char line[64] = {};
    const char expected[] = "[f3        0.050s] E test   only to nnh\n";
    CHECK(nnh.recv_all(line, sizeof expected - 1) && std::strcmp(line, expected) == 0);
    platform::shutdown();
}

NNH_TEST(pc_remote_closes_its_connection_at_shutdown) {
    if constexpr (!nnh::build::kDevToolsEnabled) return; // Release never connects
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    const std::string address = "127.0.0.1:" + std::to_string(listener.local_port());
    set_remote(address.c_str());
    CHECK(platform::init(config()));
    set_remote(nullptr);
    net::Socket nnh = net::tcp_accept(listener, 2000);
    CHECK(nnh.valid());
    platform::shutdown();
    nnh.set_recv_timeout(2000);
    char byte = 0;
    CHECK(nnh.recv_some(&byte, 1) == 0); // nnh sees the game close
}

NNH_TEST(pc_remote_without_nnh_listening_gives_up_without_a_connection) {
    net::Socket probe = net::tcp_listen(kLoopback, 0);
    const std::string address = "127.0.0.1:" + std::to_string(probe.local_port());
    probe.close(); // nobody listens there now
    set_remote(address.c_str());
    CHECK(platform::init(config()));
    set_remote(nullptr);
    unsigned char buffer[16] = {};
    CHECK(platform::remote_receive(buffer, sizeof buffer) == -1);
    platform::shutdown();
}

NNH_TEST(pc_remote_capture_reads_each_screen_back_as_the_frame_drew_it) {
    CHECK(platform::init(config()));
    CHECK(platform::poll_events());
    platform::begin_frame(nnh::Color{200, 10, 20, 255}, nnh::Color{5, 60, 210, 255});
    platform::DrawItem2D mark;
    mark.kind = platform::DrawItem2D::Kind::FillRect;
    mark.x = 10;
    mark.y = 20;
    mark.w = 4;
    mark.h = 3;
    mark.color = nnh::Color{0, 255, 0, 255};
    platform::draw_2d(nnh::Screen::Top, &mark, 1);
    platform::end_frame();

    platform::ScreenPixels top;
    CHECK(platform::remote_capture(nnh::Screen::Top, top));
    CHECK(top.width == 400 && top.height == 240 && top.layout == platform::PixelLayout::Rgba8Rows);
    CHECK(top.size == 400u * 240u * 4u && top.data != nullptr);
    CHECK(pixel_is(top, 0, 0, 200, 10, 20) && pixel_is(top, 399, 239, 200, 10, 20));
    CHECK(pixel_is(top, 10, 20, 0, 255, 0) && pixel_is(top, 13, 22, 0, 255, 0)); // rows top-down, not flipped
    CHECK(pixel_is(top, 14, 22, 200, 10, 20) && pixel_is(top, 13, 23, 200, 10, 20));

    platform::ScreenPixels bottom;
    CHECK(platform::remote_capture(nnh::Screen::Bottom, bottom));
    CHECK(bottom.width == 320 && bottom.height == 240 && bottom.size == 320u * 240u * 4u);
    CHECK(pixel_is(bottom, 0, 0, 5, 60, 210) && pixel_is(bottom, 319, 239, 5, 60, 210));
    platform::remote_capture_release();
    platform::remote_capture_release(); // twice: safe
    platform::shutdown();
}
