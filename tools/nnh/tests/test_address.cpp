// IPv4 parsing and the local-network rule (address.hpp, spec 010 FR-7).

#include "address.hpp"
#include "test.hpp"

using namespace nnh::tool;

NNH_TEST(ipv4_parses_dotted_quads_only) {
    const auto a = parse_ipv4("192.168.10.52");
    CHECK(a && a->b[0] == 192 && a->b[3] == 52);
    CHECK(to_string(*a) == "192.168.10.52");
    CHECK(parse_ipv4("0.0.0.0").has_value());
    for (const char* bad : {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "1.2.3.-4", "a.b.c.d", "01.2.3.4", "1..2.3",
                            " 1.2.3.4", "1.2.3.4 ", "console.local", "1.2.3.4:5000"}) {
        CHECK(!parse_ipv4(bad).has_value());
    }
}

NNH_TEST(broadcast_addresses_come_from_the_prefix) {
    CHECK(broadcast_of(*parse_ipv4("192.168.10.53"), 24) == *parse_ipv4("192.168.10.255"));
    CHECK(broadcast_of(*parse_ipv4("10.1.2.3"), 8) == *parse_ipv4("10.255.255.255"));
    CHECK(broadcast_of(*parse_ipv4("172.17.112.1"), 20) == *parse_ipv4("172.17.127.255"));
    CHECK(broadcast_of(*parse_ipv4("10.0.0.7"), 32) == *parse_ipv4("10.0.0.7"));
    CHECK(broadcast_of(*parse_ipv4("10.0.0.7"), 0) == *parse_ipv4("255.255.255.255"));
}

NNH_TEST(only_the_local_network_is_allowed) {
    for (const char* ok : {"10.0.0.1", "172.16.0.1", "172.31.255.255", "192.168.10.52", "169.254.1.2", "127.0.0.1"}) {
        CHECK(is_local_network(*parse_ipv4(ok)));
    }
    for (const char* no : {"8.8.8.8", "172.15.0.1", "172.32.0.1", "192.169.0.1", "1.1.1.1", "100.64.0.1"}) {
        CHECK(!is_local_network(*parse_ipv4(no)));
    }
}
