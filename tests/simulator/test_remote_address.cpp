// NNH_REMOTE for the simulator (engine/platforms/pc/src/remote_address_pc.hpp, spec 013 FR-4, FR-7): only a
// loopback address with a port is accepted; anything else is ignored, so a game never connects elsewhere.

#include "remote_address_pc.hpp"
#include "test.hpp"

using nnh::platform::pc::parse_remote_address;
using nnh::platform::pc::RemoteAddress;

namespace {

bool rejects(const char* text) {
    RemoteAddress a;
    return !parse_remote_address(text, a);
}

} // namespace

NNH_TEST(remote_address_takes_a_loopback_ip_and_port) {
    RemoteAddress a;
    CHECK(parse_remote_address("127.0.0.1:17500", a));
    CHECK(a.ip[0] == 127 && a.ip[1] == 0 && a.ip[2] == 0 && a.ip[3] == 1 && a.port == 17500);
    CHECK(parse_remote_address("127.255.2.3:1", a));
    CHECK(a.ip[1] == 255 && a.ip[2] == 2 && a.ip[3] == 3 && a.port == 1);
    CHECK(parse_remote_address("127.0.0.1:65535", a) && a.port == 65535);
}

NNH_TEST(remote_address_refuses_every_other_network) {
    CHECK(rejects("192.168.10.52:17491"));
    CHECK(rejects("10.0.0.1:5000"));
    CHECK(rejects("0.0.0.0:5000"));
    CHECK(rejects("128.0.0.1:5000"));
    CHECK(rejects("126.0.0.1:5000"));
}

NNH_TEST(remote_address_refuses_malformed_text) {
    CHECK(rejects(nullptr));
    CHECK(rejects(""));
    CHECK(rejects("127.0.0.1"));
    CHECK(rejects("127.0.0.1:"));
    CHECK(rejects("127.0.0.1:0"));
    CHECK(rejects("127.0.0.1:65536"));
    CHECK(rejects("127.0.0.1:123456"));
    CHECK(rejects("127.0.0.1:017491")); // at most 5 digits
    CHECK(rejects("127.0.0.1:80x"));
    CHECK(rejects("127.0.0.1:-1"));
    CHECK(rejects("127.0.0.256:80"));
    CHECK(rejects("127.0.0:80"));
    CHECK(rejects("127.0.0.1.5:80"));
    CHECK(rejects("127..0.1:80"));
    CHECK(rejects("127.0.0.1000:80"));
    CHECK(rejects("localhost:80"));
    CHECK(rejects(" 127.0.0.1:80"));
    CHECK(rejects("127.0.0.1 :80"));
}
