#include "net.hpp"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
using NativeSocket = SOCKET;
#define NNH_CLOSE_SOCKET closesocket
#define NNH_SEND_FLAGS 0
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using NativeSocket = int;
#define NNH_CLOSE_SOCKET ::close
#define NNH_SEND_FLAGS MSG_NOSIGNAL
#endif

namespace nnh::tool::net {

namespace {

NativeSocket native(std::intptr_t h) { return static_cast<NativeSocket>(h); }
std::intptr_t wrap(NativeSocket s) {
#ifdef _WIN32
    return s == INVALID_SOCKET ? -1 : static_cast<std::intptr_t>(s);
#else
    return s < 0 ? -1 : static_cast<std::intptr_t>(s);
#endif
}

sockaddr_in to_sockaddr(Ipv4 a, std::uint16_t port) {
    sockaddr_in sa;
    std::memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    std::memcpy(&sa.sin_addr, a.b, 4);
    return sa;
}

Ipv4 from_sockaddr(const sockaddr_in& sa) {
    Ipv4 a;
    std::memcpy(a.b, &sa.sin_addr, 4);
    return a;
}

/// Waits until `s` is readable (or writable). @return true if ready before `timeout_ms`.
bool wait_ready(NativeSocket s, int timeout_ms, bool write) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    const int n = select(static_cast<int>(s) + 1, write ? nullptr : &set, write ? &set : nullptr, nullptr, &tv);
    return n > 0;
}

void set_blocking(NativeSocket s, bool blocking) {
#ifdef _WIN32
    u_long mode = blocking ? 0 : 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    const int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
#endif
}

} // namespace

bool startup() {
#ifdef _WIN32
    static const bool ok = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return ok;
#else
    return true;
#endif
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = -1;
    }
    return *this;
}

void Socket::close() {
    if (handle_ != -1) NNH_CLOSE_SOCKET(native(handle_));
    handle_ = -1;
}

bool Socket::send_all(const void* data, std::size_t size) {
    const char* p = static_cast<const char*>(data);
    while (size > 0) {
        const int chunk = size > 65536 ? 65536 : static_cast<int>(size);
        const auto n = ::send(native(handle_), p, chunk, NNH_SEND_FLAGS);
        if (n <= 0) return false;
        p += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

bool Socket::recv_all(void* data, std::size_t size) {
    char* p = static_cast<char*>(data);
    while (size > 0) {
        const long n = recv_some(p, size);
        if (n <= 0) return false;
        p += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

long Socket::recv_some(void* data, std::size_t size) {
    const int chunk = size > 65536 ? 65536 : static_cast<int>(size);
    const auto n = ::recv(native(handle_), static_cast<char*>(data), chunk, 0);
    return n < 0 ? -1 : static_cast<long>(n);
}

bool Socket::set_recv_timeout(int ms) {
#ifdef _WIN32
    const DWORD value = static_cast<DWORD>(ms);
    return setsockopt(native(handle_), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&value), sizeof value) == 0;
#else
    timeval tv{ms / 1000, (ms % 1000) * 1000};
    return setsockopt(native(handle_), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == 0;
#endif
}

std::uint16_t Socket::local_port() const {
    sockaddr_in sa;
    socklen_t len = sizeof sa;
    if (getsockname(native(handle_), reinterpret_cast<sockaddr*>(&sa), &len) != 0) return 0;
    return ntohs(sa.sin_port);
}

Socket tcp_connect(Ipv4 address, std::uint16_t port, int timeout_ms) {
    if (!startup()) return {};
    Socket s(wrap(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)));
    if (!s.valid()) return {};
    const sockaddr_in sa = to_sockaddr(address, port);
    set_blocking(native(s.handle()), false);
    ::connect(native(s.handle()), reinterpret_cast<const sockaddr*>(&sa), sizeof sa);
    // Windows reports a failed connect in the exception set, not the write set: wait for both.
    fd_set write_set;
    fd_set error_set;
    FD_ZERO(&write_set);
    FD_ZERO(&error_set);
    FD_SET(native(s.handle()), &write_set);
    FD_SET(native(s.handle()), &error_set);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    const int ready = select(static_cast<int>(native(s.handle())) + 1, nullptr, &write_set, &error_set, &tv);
    if (ready <= 0 || FD_ISSET(native(s.handle()), &error_set)) return {};
    int error = 0;
    socklen_t len = sizeof error;
    getsockopt(native(s.handle()), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len);
    if (error != 0) return {};
    set_blocking(native(s.handle()), true);
    return s;
}

Socket tcp_listen(Ipv4 address, std::uint16_t port) {
    if (!startup()) return {};
    Socket s(wrap(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)));
    if (!s.valid()) return {};
#ifndef _WIN32
    const int yes = 1; // rebinding 17491 right after a run must work
    setsockopt(native(s.handle()), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
#endif
    const sockaddr_in sa = to_sockaddr(address, port);
    if (::bind(native(s.handle()), reinterpret_cast<const sockaddr*>(&sa), sizeof sa) != 0) return {};
    if (::listen(native(s.handle()), 4) != 0) return {};
    return s;
}

Socket tcp_accept(const Socket& listener, int timeout_ms, Ipv4* peer) {
    if (!wait_ready(native(listener.handle()), timeout_ms, false)) return {};
    sockaddr_in sa;
    socklen_t len = sizeof sa;
    Socket s(wrap(::accept(native(listener.handle()), reinterpret_cast<sockaddr*>(&sa), &len)));
    if (s.valid() && peer != nullptr) *peer = from_sockaddr(sa);
    return s;
}

Socket udp_open(Ipv4 address, std::uint16_t port) {
    if (!startup()) return {};
    Socket s(wrap(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)));
    if (!s.valid()) return {};
    const int yes = 1;
    setsockopt(native(s.handle()), SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof yes);
    const sockaddr_in sa = to_sockaddr(address, port);
    if (::bind(native(s.handle()), reinterpret_cast<const sockaddr*>(&sa), sizeof sa) != 0) return {};
    return s;
}

bool udp_send(const Socket& socket, Ipv4 to, std::uint16_t port, const void* data, std::size_t size) {
    const sockaddr_in sa = to_sockaddr(to, port);
    const auto n = ::sendto(native(socket.handle()), static_cast<const char*>(data), static_cast<int>(size), 0,
                            reinterpret_cast<const sockaddr*>(&sa), sizeof sa);
    return static_cast<long long>(n) == static_cast<long long>(size);
}

long udp_receive(const Socket& socket, void* data, std::size_t size, int timeout_ms, Ipv4* from) {
    if (!wait_ready(native(socket.handle()), timeout_ms, false)) return -1;
    sockaddr_in sa;
    socklen_t len = sizeof sa;
    const auto n = ::recvfrom(native(socket.handle()), static_cast<char*>(data), static_cast<int>(size), 0,
                              reinterpret_cast<sockaddr*>(&sa), &len);
    if (n < 0) return -1;
    if (from != nullptr) *from = from_sockaddr(sa);
    return static_cast<long>(n);
}

std::vector<Ipv4> local_broadcast_addresses() {
    std::vector<Ipv4> out;
    auto add = [&out](Ipv4 address, int prefix) {
        if (!is_local_network(address) || address.b[0] == 127) return;
        const Ipv4 b = broadcast_of(address, prefix);
        for (const Ipv4& known : out) {
            if (known == b) return;
        }
        out.push_back(b);
    };
#ifdef _WIN32
    ULONG size = 16 * 1024;
    std::vector<unsigned char> buffer(size);
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    auto* list = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, list, &size) == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(size);
        list = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    }
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, list, &size) != NO_ERROR) return out;
    for (auto* adapter = list; adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) continue;
        for (auto* a = adapter->FirstUnicastAddress; a != nullptr; a = a->Next) {
            const auto* sa = reinterpret_cast<const sockaddr_in*>(a->Address.lpSockaddr);
            add(from_sockaddr(*sa), a->OnLinkPrefixLength);
        }
    }
#else
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return out;
    for (ifaddrs* i = list; i != nullptr; i = i->ifa_next) {
        if (i->ifa_addr == nullptr || i->ifa_netmask == nullptr || i->ifa_addr->sa_family != AF_INET) continue;
        if ((i->ifa_flags & IFF_UP) == 0 || (i->ifa_flags & IFF_BROADCAST) == 0) continue;
        const std::uint32_t mask = ntohl(reinterpret_cast<const sockaddr_in*>(i->ifa_netmask)->sin_addr.s_addr);
        int prefix = 0;
        while (prefix < 32 && (mask & (0x80000000u >> prefix)) != 0) ++prefix;
        add(from_sockaddr(*reinterpret_cast<const sockaddr_in*>(i->ifa_addr)), prefix);
    }
    freeifaddrs(list);
#endif
    return out;
}

std::optional<Ipv4> local_address_towards(Ipv4 host) {
    if (!startup()) return std::nullopt;
    Socket s(wrap(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)));
    const sockaddr_in sa = to_sockaddr(host, 9);
    if (!s.valid() || ::connect(native(s.handle()), reinterpret_cast<const sockaddr*>(&sa), sizeof sa) != 0) {
        return std::nullopt;
    }
    sockaddr_in local;
    socklen_t len = sizeof local;
    if (getsockname(native(s.handle()), reinterpret_cast<sockaddr*>(&local), &len) != 0) return std::nullopt;
    return from_sockaddr(local);
}

} // namespace nnh::tool::net
