// Nintendo 3DS platform: the console remote's connection (contract nnh/platform/remote.hpp, spec 013 T5.1).
// The remote uses the log socket app_3ds.cpp opened to the 3dslink host (ctr::link_socket(); stdout and stderr
// are dup2'd onto it), so log lines and records travel in one ordered stream. Reads never wait (MSG_DONTWAIT);
// a send flushes stdio first, then blocks until written, in 16 KB pieces. The screen read-back is
// remote_capture_3ds.cpp. Debug/Development only: core never calls this in Release, where there is no socket.

#include <3ds.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>

#include "context_3ds.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/remote.hpp"

namespace nnh::platform {
namespace {

constexpr std::size_t kSendPiece = 16 * 1024;

/// The log socket while it works, else -1.
int live_socket() {
    const int s = ctr::link_socket();
    return ctr::context().remote_closed ? -1 : s;
}

int gone() {
    ctr::context().remote_closed = true; // core stops calling after -1; a send fails from now on
    return -1;
}

/// Ticks and replies are a few bytes: once nnh talks to us, don't hold them back (Nagle). Set at the first
/// remote send, so a log-only connection (`3dslink -s`, `nnh serve`) is left as it always was.
void no_delay_once(int s) {
    ctr::Context& ctx = ctr::context();
    if (ctx.remote_no_delay) return;
    ctx.remote_no_delay = true;
    const int yes = 1;
    setsockopt(s, SOL_TCP, TCP_NODELAY, &yes, sizeof yes); // best effort: only latency depends on it
}

} // namespace

long remote_receive(void* buffer, std::size_t size) {
    if constexpr (!build::kDevToolsEnabled) return -1;
    const int s = live_socket();
    if (s < 0) return -1;
    if (size == 0) return 0;
    const ssize_t n = recv(s, buffer, size, MSG_DONTWAIT);
    if (n > 0) return static_cast<long>(n);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return 0;
    return gone(); // 0: the PC closed the connection; < 0: it broke
}

bool remote_send(const void* data, std::size_t size) {
    if constexpr (!build::kDevToolsEnabled) return false;
    const int s = live_socket();
    if (s < 0) return false;
    std::fflush(stdout); // stdout is fully buffered on the socket: earlier log lines go first
    std::fflush(stderr);
    no_delay_once(s);
    const auto* p = static_cast<const std::uint8_t*>(data);
    while (size > 0) {
        const std::size_t piece = size < kSendPiece ? size : kSendPiece;
        const ssize_t n = send(s, p, piece, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            gone();
            return false;
        }
        p += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

} // namespace nnh::platform
