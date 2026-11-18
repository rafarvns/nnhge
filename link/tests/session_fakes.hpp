#pragma once
// Fakes for the session tests (test_sessions.cpp, test_session_failures.cpp): the server's hooks, the
// console's pair store and file sink, and a loopback pair that runs serve_console on a thread.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "nnh_link/client_session.hpp"
#include "nnh_link/server_session.hpp"

namespace fakes {

using namespace nnh::link;

inline const Ipv4 kLoopback{{127, 0, 0, 1}};

inline Hello hello_of(std::uint8_t id, std::uint8_t nonce) {
    Hello h;
    h.id.fill(id);
    h.nonce.fill(nonce);
    return h;
}

struct Pairs {
    std::vector<std::pair<Id, Digest>> list;
    std::optional<Digest> find(const Id& id) const {
        for (const auto& p : list) {
            if (p.first == id) return p.second;
        }
        return std::nullopt;
    }
    void put(const Id& id, const Digest& d) {
        for (auto& p : list) {
            if (p.first == id) {
                p.second = d;
                return;
            }
        }
        list.emplace_back(id, d);
    }
};

class Hooks : public ServerHooks {
public:
    Pairs pairs;
    std::string code = "123456";
    std::vector<GameInfo> game_list = {{"hello", "Hello NNHGE"}, {"audio", "NNHGE Audio"}};
    std::vector<std::string> output = {"[1/2] compiling", "[2/2] linking"};
    bool build_ok = true;
    std::string file;              // the built file
    int line_delay_ms = 0;         // pause before each line
    int silent_ms = 0;             // pause before building
    std::atomic<int> lines_sent{0};
    std::atomic<int> builds{0};
    std::string launched;

    std::optional<Digest> secret_of(const Id& console) override { return pairs.find(console); }
    void remember(const Id& console, const Digest& secret) override { pairs.put(console, secret); }
    std::string pairing_code() override { return code; }
    std::vector<GameInfo> games() override { return game_list; }
    BuildOutput build(const BuildRequest&, const std::function<bool(const std::string&)>& line) override {
        ++builds;
        std::this_thread::sleep_for(std::chrono::milliseconds(silent_ms));
        for (const auto& l : output) {
            std::this_thread::sleep_for(std::chrono::milliseconds(line_delay_ms));
            if (!line(l)) return {};
            ++lines_sent;
        }
        BuildOutput out;
        out.ok = build_ok;
        out.file = file;
        if (!build_ok) out.lines = output;
        return out;
    }
    void launching(const std::string& game) override { launched = game; }
};

class Store : public PairStore {
public:
    Pairs pairs;
    std::optional<Digest> secret_of(const Id& server) override { return pairs.find(server); }
    void remember(const Id& server, const Digest& secret) override { pairs.put(server, secret); }
};

class Sink : public FileSink {
public:
    std::vector<std::uint8_t> data;
    std::uint64_t space = 1ull << 30;
    bool begun = false, kept = false, dropped = false;
    bool begin(std::uint64_t size) override {
        begun = true;
        return size <= space;
    }
    bool write(const std::uint8_t* p, std::size_t n) override {
        data.insert(data.end(), p, p + n);
        return true;
    }
    void finish(bool ok) override { (ok ? kept : dropped) = true; }
};

/// A file of `size` pseudo-random bytes in the build folder. @return its path.
inline std::string make_file(const char* name, std::size_t size) {
    const std::string path = std::string(name) + ".3dsx";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    for (std::size_t i = 0; i < size; ++i) std::fputc(static_cast<int>((i * 2654435761u) >> 13) & 0xFF, f);
    std::fclose(f);
    return path;
}

/// One server session on a thread. end() joins it and returns why it ended.
class Server {
public:
    Server(Hooks& hooks, PairingGuard& guard, const Hello& mine, ServeLimits limits = {})
        : listener_(net::tcp_listen(kLoopback, 0)) {
        thread_ = std::thread([this, &hooks, &guard, mine, limits] {
            net::Socket s = net::tcp_accept(listener_, 5000);
            result_ = serve_console(static_cast<net::Socket&&>(s), mine, hooks, guard, limits);
        });
    }
    ~Server() {
        if (thread_.joinable()) thread_.join();
    }
    net::Socket connect() { return net::tcp_connect(kLoopback, listener_.local_port(), 5000); }
    std::string end() {
        if (thread_.joinable()) thread_.join();
        return result_;
    }

private:
    net::Socket listener_;
    std::thread thread_;
    std::string result_;
};

/// Updates until the session leaves Handshake/Busy (or 5 s pass).
inline ClientSession::State settle(ClientSession& c) {
    for (int i = 0; i < 500; ++i) {
        const auto s = c.update(10);
        if (s != ClientSession::State::Handshake && s != ClientSession::State::Busy) return s;
    }
    return c.state();
}

} // namespace fakes
