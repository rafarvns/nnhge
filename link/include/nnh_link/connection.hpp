#pragma once
// A TCP connection that speaks NNH Link frames (spec 011): plain during the handshake, sealed (sequence +
// MAC, frame.hpp) once secure() is called. receive() never waits longer than asked, so the console's frame
// loop can call it with 0 every frame. Tested on the loopback interface (link/tests/test_sessions.cpp).

#include <cstdint>
#include <optional>
#include <vector>

#include "nnh_link/frame.hpp"
#include "nnh_link/net.hpp"

namespace nnh::link {

class Connection {
public:
    Connection() = default;
    explicit Connection(net::Socket socket) : socket_(static_cast<net::Socket&&>(socket)) {}

    bool open() const { return socket_.valid(); }
    void close() { socket_.close(); }

    /// Seals every later frame with `key` (and expects sealed frames from the other side).
    void secure(const Digest& key, Side me) { channel_.emplace(key, me); }
    bool secured() const { return channel_.has_value(); }

    /// Sends a message, sealed once secured. @return false if the connection broke.
    bool send(Type type, const std::vector<std::uint8_t>& message);
    /// Sends an already encoded plain frame (the handshake's output).
    bool send_frame(const std::vector<std::uint8_t>& frame);

    enum class Status { Frame, Waiting, Closed, Broken };
    /// The next frame, waiting up to `timeout_ms` (0 = only what already arrived). A sealed frame is opened:
    /// `out.payload` is the message. Broken = malformed or forged data; the connection is closed then.
    Status receive(Frame& out, int timeout_ms);

private:
    Status take(Frame& out);

    net::Socket socket_;
    FrameReader reader_;
    std::optional<SecureChannel> channel_;
};

} // namespace nnh::link
