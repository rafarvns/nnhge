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

void LogListener::read_lines(void (*on_line)(const std::string& line, void* context), void* context) {
    std::string pending;
    char buffer[4096];
    auto emit = [&](std::string line) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        on_line(line, context);
    };
    for (;;) {
        const long n = game_.recv_some(buffer, sizeof buffer);
        if (n <= 0) break;
        pending.append(buffer, static_cast<std::size_t>(n));
        std::size_t start = 0;
        for (std::size_t nl = pending.find('\n'); nl != std::string::npos; nl = pending.find('\n', start)) {
            emit(pending.substr(start, nl - start));
            start = nl + 1;
        }
        pending.erase(0, start);
    }
    if (!pending.empty()) emit(pending);
    game_.close();
}

} // namespace nnh::tool
