#pragma once
// Small blocking sockets for nnh (spec 010): TCP and UDP over IPv4 with timeouts, on Winsock and BSD
// sockets. A host-tool module: platform #ifdefs live in net.cpp only. Tested on the loopback interface in
// tools/nnh/tests/test_net.cpp.

#include <cstddef>
#include <cstdint>
#include <optional>

#include "address.hpp"

namespace nnh::tool::net {

/// Starts the socket library once (WSAStartup on Windows). @return false if it can't.
bool startup();

class Socket {
public:
    Socket() = default;
    explicit Socket(std::intptr_t handle) : handle_(handle) {}
    ~Socket() { close(); }
    Socket(Socket&& other) noexcept : handle_(other.handle_) { other.handle_ = -1; }
    Socket& operator=(Socket&& other) noexcept;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    bool valid() const { return handle_ != -1; }
    void close();
    std::intptr_t handle() const { return handle_; }

    /// Sends everything. @return false on error.
    bool send_all(const void* data, std::size_t size);
    /// Receives exactly `size` bytes. @return false on error, timeout or a closed connection.
    bool recv_all(void* data, std::size_t size);
    /// Receives up to `size` bytes: > 0 bytes, 0 the peer closed, -1 error or timeout.
    long recv_some(void* data, std::size_t size);
    /// Receive timeout for later recv calls (0 = none).
    bool set_recv_timeout(int ms);
    /// The local port it is bound to (0 if unknown).
    std::uint16_t local_port() const;

private:
    std::intptr_t handle_ = -1;
};

/// Connects within `timeout_ms`. Invalid socket on failure (refused, unreachable, timeout).
Socket tcp_connect(Ipv4 address, std::uint16_t port, int timeout_ms);

/// Listens on `address` (0.0.0.0 = every interface) and `port` (0 = any free port).
Socket tcp_listen(Ipv4 address, std::uint16_t port);

/// Waits up to `timeout_ms` for a connection. Invalid socket on timeout or error; `peer` receives the
/// other side's address.
Socket tcp_accept(const Socket& listener, int timeout_ms, Ipv4* peer = nullptr);

/// A UDP socket bound to `address`:`port` (0 = any), with broadcast enabled.
Socket udp_open(Ipv4 address, std::uint16_t port);
bool udp_send(const Socket& socket, Ipv4 to, std::uint16_t port, const void* data, std::size_t size);
/// Waits up to `timeout_ms` for a datagram: its size, or -1 on timeout or error.
long udp_receive(const Socket& socket, void* data, std::size_t size, int timeout_ms, Ipv4* from = nullptr);

/// This computer's address on the route to `host` (no packet is sent).
std::optional<Ipv4> local_address_towards(Ipv4 host);

} // namespace nnh::tool::net
