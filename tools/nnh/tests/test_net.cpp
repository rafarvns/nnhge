// Sockets (net.hpp, spec 010) on the loopback interface only: no firewall prompt, no network needed.

#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>

#include "net.hpp"
#include "test.hpp"

using namespace nnh::tool;

namespace {

const Ipv4 kLoopback{{127, 0, 0, 1}};

} // namespace

NNH_TEST(tcp_round_trip_on_loopback) {
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    CHECK(listener.valid() && listener.local_port() != 0);
    std::thread echo([&] {
        net::Socket client = net::tcp_accept(listener, 5000);
        char buffer[5];
        if (client.recv_all(buffer, 5)) client.send_all(buffer, 5);
    });
    net::Socket s = net::tcp_connect(kLoopback, listener.local_port(), 5000);
    CHECK(s.valid() && s.send_all("hello", 5));
    char back[5] = {};
    CHECK(s.recv_all(back, 5) && std::memcmp(back, "hello", 5) == 0);
    echo.join();
    CHECK(s.recv_some(back, 5) == 0); // the other side closed
}

NNH_TEST(tcp_connect_to_a_closed_port_fails) {
    net::Socket probe = net::tcp_listen(kLoopback, 0);
    const std::uint16_t port = probe.local_port();
    probe.close(); // nothing listens there now
    CHECK(!net::tcp_connect(kLoopback, port, 300).valid());
}

NNH_TEST(a_refused_connection_fails_before_its_timeout) {
    // Windows reports a refused connect in select()'s exception set; waiting only for "writable" would
    // sit out the whole timeout (5 s per try in nnh run).
    net::Socket probe = net::tcp_listen(kLoopback, 0);
    const std::uint16_t port = probe.local_port();
    probe.close();
    const auto start = std::chrono::steady_clock::now();
    CHECK(!net::tcp_connect(kLoopback, port, 5000).valid());
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(elapsed < std::chrono::milliseconds(4000));
}

NNH_TEST(tcp_accept_and_receive_time_out) {
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    CHECK(!net::tcp_accept(listener, 50).valid());
    std::thread silent([&] {
        net::Socket client = net::tcp_accept(listener, 5000);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    });
    net::Socket s = net::tcp_connect(kLoopback, listener.local_port(), 2000);
    CHECK(s.set_recv_timeout(50));
    char c;
    CHECK(s.recv_some(&c, 1) == -1);
    silent.join();
}

NNH_TEST(udp_datagrams_carry_their_sender) {
    net::Socket a = net::udp_open(kLoopback, 0);
    net::Socket b = net::udp_open(kLoopback, 0);
    CHECK(a.valid() && b.valid());
    CHECK(net::udp_send(a, kLoopback, b.local_port(), "3dsboot", 7));
    char buffer[16];
    Ipv4 from;
    CHECK(net::udp_receive(b, buffer, sizeof buffer, 2000, &from) == 7 && from == kLoopback);
    CHECK(net::udp_receive(b, buffer, sizeof buffer, 30) == -1); // nothing else: timeout
}

NNH_TEST(local_broadcasts_are_local_networks_without_loopback) {
    const auto broadcasts = net::local_broadcast_addresses();
    for (const Ipv4& b : broadcasts) {
        CHECK(is_local_network(b) && b.b[0] != 127);
        std::printf("  broadcast %s\n", to_string(b).c_str());
    }
}

NNH_TEST(local_address_towards_loopback_is_loopback) {
    const auto local = net::local_address_towards(kLoopback);
    CHECK(local && *local == kLoopback);
}
