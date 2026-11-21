// PC simulator: the console remote's connection (contract nnh/platform/remote.hpp, spec 013 T4.1). The
// simulator connects back to the `nnh run --pc` named by NNH_REMOTE (loopback only, see remote_address_pc.hpp);
// reads never wait, sends block until written. While connected the log goes to nnh too (log_pc.cpp), so log
// lines and records share one ordered stream, as on the console. The screen read-back is remote_capture_pc.cpp.

#include "remote_pc.hpp"

#include <cstdlib>
#include <cstring>

#include "nnh/platform/remote.hpp"
#include "remote_address_pc.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using NativeSocket = SOCKET;
constexpr NativeSocket kNoSocket = INVALID_SOCKET;
#define NNH_CLOSE_SOCKET closesocket
#define NNH_SEND_FLAGS 0
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
using NativeSocket = int;
constexpr NativeSocket kNoSocket = -1;
#define NNH_CLOSE_SOCKET ::close
#define NNH_SEND_FLAGS MSG_NOSIGNAL
#endif

namespace nnh::platform {
namespace {

constexpr int kConnectTimeoutMs = 1000;

NativeSocket g_socket = kNoSocket;

void set_blocking(NativeSocket s, bool blocking) {
#if defined(_WIN32)
    u_long mode = blocking ? 0 : 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    const int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
#endif
}

/// Waits up to `timeout_ms` for `s` to be readable (or writable). A closed connection counts as readable.
bool ready(NativeSocket s, int timeout_ms, bool write) {
#if defined(_WIN32)
    fd_set set;
    fd_set errors;
    FD_ZERO(&set);
    FD_ZERO(&errors);
    FD_SET(s, &set);
    FD_SET(s, &errors);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    const int n = select(0, write ? nullptr : &set, write ? &set : nullptr, &errors, &tv);
    return n > 0 && FD_ISSET(s, &set) && !FD_ISSET(s, &errors);
#else
    pollfd p{s, static_cast<short>(write ? POLLOUT : POLLIN), 0};
    const int n = poll(&p, 1, timeout_ms);
    if (n <= 0) return false;
    return write ? (p.revents & POLLOUT) != 0 && (p.revents & (POLLERR | POLLHUP)) == 0 : true;
#endif
}

/// Gives up a connection attempt: closes `s` (if any) and balances the WSAStartup of connect_to().
NativeSocket give_up(NativeSocket s) {
    if (s != kNoSocket) NNH_CLOSE_SOCKET(s);
#if defined(_WIN32)
    WSACleanup();
#endif
    return kNoSocket;
}

NativeSocket connect_to(const pc::RemoteAddress& address) {
#if defined(_WIN32)
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return kNoSocket;
#endif
    NativeSocket s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kNoSocket) return give_up(s);
    sockaddr_in sa;
    std::memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(address.port);
    std::memcpy(&sa.sin_addr, address.ip, 4);
    set_blocking(s, false);
    ::connect(s, reinterpret_cast<const sockaddr*>(&sa), sizeof sa);
    if (!ready(s, kConnectTimeoutMs, true)) return give_up(s);
    set_blocking(s, true);
    const int yes = 1; // ticks and log lines are small: don't hold them back (Nagle)
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&yes), sizeof yes);
    return s;
}

bool send_bytes(const void* data, std::size_t size) {
    const char* p = static_cast<const char*>(data);
    while (size > 0) {
        const int chunk = size > 65536 ? 65536 : static_cast<int>(size);
        const auto n = ::send(g_socket, p, chunk, NNH_SEND_FLAGS);
        if (n <= 0) {
            pc::remote_close(); // nnh is gone: the log goes back to the terminal
            return false;
        }
        p += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

} // namespace

void pc::remote_open() {
    remote_close();
    RemoteAddress address;
    if (parse_remote_address(std::getenv("NNH_REMOTE"), address)) g_socket = connect_to(address);
}

void pc::remote_close() {
    if (g_socket == kNoSocket) return;
    g_socket = give_up(g_socket);
}

bool pc::remote_log_line(const char* line) {
    if (g_socket == kNoSocket) return false;
    char buffer[512];
    const std::size_t length = std::strlen(line);
    if (length < sizeof buffer) { // one send: a line never arrives in two pieces when it fits
        std::memcpy(buffer, line, length);
        buffer[length] = '\n';
        return send_bytes(buffer, length + 1);
    }
    return send_bytes(line, length) && send_bytes("\n", 1);
}

long remote_receive(void* buffer, std::size_t size) {
    if (g_socket == kNoSocket) return -1;
    if (!ready(g_socket, 0, false)) return 0;
    const int chunk = size > 65536 ? 65536 : static_cast<int>(size);
    const auto n = ::recv(g_socket, static_cast<char*>(buffer), chunk, 0);
    if (n <= 0) { // closed (0) or broken
        pc::remote_close();
        return -1;
    }
    return static_cast<long>(n);
}

bool remote_send(const void* data, std::size_t size) {
    return g_socket != kNoSocket && send_bytes(data, size);
}

} // namespace nnh::platform
