#include "file_server.hpp"

#include <cstdio>

namespace nnh::tool {

bool FileServer::start(Ipv4 address, std::uint16_t port, std::map<std::string, std::string> files) {
    stop();
    listener_ = net::tcp_listen(address, port);
    if (!listener_.valid()) return false;
    port_ = listener_.local_port();
    files_ = std::move(files);
    running_ = true;
    thread_ = std::thread([this] { serve(); });
    return true;
}

void FileServer::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    listener_.close();
}

void FileServer::serve() {
    while (running_) {
        net::Socket client = net::tcp_accept(listener_, 100); // short waits: stop() is seen quickly
        if (client.valid()) answer(client);
    }
}

void FileServer::answer(net::Socket& client) {
    client.set_recv_timeout(5000);
    std::string request;
    char buffer[1024];
    while (request.find("\r\n\r\n") == std::string::npos && request.size() < 8192) {
        const long n = client.recv_some(buffer, sizeof buffer);
        if (n <= 0) return;
        request.append(buffer, static_cast<std::size_t>(n));
    }
    // "GET /path HTTP/1.x"
    const std::size_t sp1 = request.find(' ');
    const std::size_t sp2 = sp1 == std::string::npos ? sp1 : request.find(' ', sp1 + 1);
    const std::string method = request.substr(0, sp1);
    const std::string path = sp2 == std::string::npos ? "" : request.substr(sp1 + 1, sp2 - sp1 - 1);
    const auto file = files_.find(path);
    std::FILE* f = method == "GET" && file != files_.end() ? std::fopen(file->second.c_str(), "rb") : nullptr;
    if (f == nullptr) {
        static const char kNotFound[] = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        client.send_all(kNotFound, sizeof kNotFound - 1);
        return;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    char header[160];
    const int n = std::snprintf(header, sizeof header,
                                "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %ld\r\n"
                                "Connection: close\r\n\r\n",
                                size);
    bool ok = client.send_all(header, static_cast<std::size_t>(n));
    static char chunk[64 * 1024]; // only the server thread uses it
    std::size_t read = 0;
    while (ok && (read = std::fread(chunk, 1, sizeof chunk, f)) > 0) ok = client.send_all(chunk, read);
    std::fclose(f);
    if (ok) ++served_;
}

} // namespace nnh::tool
