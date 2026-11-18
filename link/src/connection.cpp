#include "nnh_link/connection.hpp"

#include <chrono>

namespace nnh::link {

bool Connection::send(Type type, const std::vector<std::uint8_t>& message) {
    return send_frame(channel_ ? channel_->seal(type, message) : encode_frame(type, message));
}

bool Connection::send_frame(const std::vector<std::uint8_t>& frame) {
    if (!socket_.valid()) return false;
    if (socket_.send_all(frame.data(), frame.size())) return true;
    socket_.close();
    return false;
}

Connection::Status Connection::take(Frame& out) {
    bool error = false;
    auto frame = reader_.next(error);
    if (error) {
        socket_.close();
        return Status::Broken;
    }
    if (!frame) return Status::Waiting;
    if (channel_) {
        auto message = channel_->open(*frame);
        if (!message) {
            socket_.close();
            return Status::Broken;
        }
        frame->payload = static_cast<std::vector<std::uint8_t>&&>(*message);
    }
    out = static_cast<Frame&&>(*frame);
    return Status::Frame;
}

Connection::Status Connection::receive(Frame& out, int timeout_ms) {
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        const Status status = take(out);
        if (status != Status::Waiting) return status;
        if (!socket_.valid()) return Status::Closed;
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if (!net::readable(socket_, left > 0 ? static_cast<int>(left) : 0)) return Status::Waiting;
        std::uint8_t buffer[4096];
        const long n = socket_.recv_some(buffer, sizeof buffer);
        if (n <= 0) {
            socket_.close();
            return Status::Closed;
        }
        reader_.feed(buffer, static_cast<std::size_t>(n));
    }
}

} // namespace nnh::link
