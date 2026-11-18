#include "console_link.hpp"

#include <cstring>

#include "fbi.hpp"
#include "link_types.hpp"
#include "netloader.hpp"
#include "zlib_stored.hpp"

namespace nnh::tool {

std::optional<Ipv4> discover_console(const std::vector<Ipv4>& targets, std::uint16_t port, std::uint16_t reply_port,
                                     int tries, int wait_ms) {
    net::Socket udp = net::udp_open(Ipv4{}, reply_port);
    if (!udp.valid()) return std::nullopt;
    for (int i = 0; i < tries; ++i) {
        for (const Ipv4& target : targets) net::udp_send(udp, target, port, kDiscoveryRequest, sizeof kDiscoveryRequest - 1);
        char reply[64];
        Ipv4 from;
        const long n = net::udp_receive(udp, reply, sizeof reply, wait_ms, &from);
        if (n > 0 && is_discovery_reply(reply, static_cast<std::size_t>(n))) return from;
    }
    return std::nullopt;
}

namespace {

bool read_status(net::Socket& s, std::int32_t& status) {
    std::uint8_t b[4];
    if (!s.recv_all(b, 4)) return false;
    status = static_cast<std::int32_t>(b[0] | b[1] << 8 | b[2] << 16 | std::uint32_t(b[3]) << 24);
    return true;
}

} // namespace

std::string send_3dsx(Ipv4 console, std::uint16_t port, const std::string& name, const std::vector<std::uint8_t>& file,
                      const std::vector<std::string>& extra_args, int connect_timeout_ms) {
    if (!valid_netloader_name(name)) return "invalid file name '" + name + "'";
    net::Socket s = net::tcp_connect(console, port, connect_timeout_ms);
    if (!s.valid()) return "can't connect to " + to_string(console) + ": is the Homebrew Launcher's netloader open (Y)?";
    s.set_recv_timeout(30000);
    const auto header = netloader_header(name, static_cast<std::uint32_t>(file.size()));
    std::int32_t status = 0;
    if (!s.send_all(header.data(), header.size()) || !read_status(s, status)) return "the console closed the connection";
    if (status < 0) return "the console couldn't create sdmc:/3ds/" + name;
    for (const auto& chunk : netloader_chunks(zlib_stored(file.data(), file.size()))) {
        if (!s.send_all(chunk.data(), chunk.size())) return "the transfer was interrupted";
    }
    if (!read_status(s, status) || status < 0) return "the console didn't confirm the file";
    std::vector<std::string> args = {netloader_path(name)};
    args.insert(args.end(), extra_args.begin(), extra_args.end());
    const auto message = netloader_args(args);
    if (!s.send_all(message.data(), message.size())) return "couldn't send the arguments";
    return "";
}

std::string fbi_send_urls(Ipv4 console, std::uint16_t port, const std::vector<std::string>& urls, int timeout_ms,
                          int connect_timeout_ms) {
    net::Socket s = net::tcp_connect(console, port, connect_timeout_ms);
    if (!s.valid()) return "can't connect to FBI at " + to_string(console) + ": is it on 'Receive URLs over the network'?";
    const auto message = fbi_url_message(urls);
    if (!s.send_all(message.data(), message.size())) return "couldn't send the URLs to FBI";
    s.set_recv_timeout(timeout_ms);
    char answer;
    const long n = s.recv_some(&answer, 1);
    return n >= 0 ? "" : "FBI didn't answer in time";
}

} // namespace nnh::tool
