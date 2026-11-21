#include "log_listener.hpp"

namespace nnh::tool {

bool LogListener::open(Ipv4 address, std::uint16_t port) {
    listener_ = net::tcp_listen(address, port);
    return listener_.valid();
}

bool LogListener::wait_for_game(int timeout_ms) {
    game_ = net::tcp_accept(listener_, timeout_ms);
    listener_.close(); // one game per run
    return game_.valid();
}

net::Socket LogListener::take_game() { return static_cast<net::Socket&&>(game_); }

} // namespace nnh::tool
