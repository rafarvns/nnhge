#pragma once
// A minimal HTTP/1.0 file server for FBI downloads (spec 010 FR-6, FR-7): serves only the files it was
// given, by exact path ("/audio.cia"), on one address, from a background thread, until stop(). Anything
// else gets 404. Tested in tools/nnh/tests/test_console_link.cpp.

#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <thread>

#include "link_types.hpp"

namespace nnh::tool {

class FileServer {
public:
    ~FileServer() { stop(); }

    /// `files`: URL path ("/name") -> file on disk. `port` 0 = any free port.
    bool start(Ipv4 address, std::uint16_t port, std::map<std::string, std::string> files);
    std::uint16_t port() const { return port_; }
    /// Files sent completely so far.
    int served() const { return served_; }
    void stop();

private:
    void serve();
    void answer(net::Socket& client);

    net::Socket listener_;
    std::map<std::string, std::string> files_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<int> served_{0};
    std::uint16_t port_ = 0;
};

} // namespace nnh::tool
