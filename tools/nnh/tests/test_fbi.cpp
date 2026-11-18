// FBI's URL message and the URLs nnh serves (fbi.hpp, spec 010 FR-6).

#include <cstring>

#include "fbi.hpp"
#include "test.hpp"

using namespace nnh::tool;

NNH_TEST(fbi_message_is_big_endian_length_and_lines) {
    const auto m = fbi_url_message({"http://a/x.cia", "http://a/y.cia"});
    const char body[] = "http://a/x.cia\nhttp://a/y.cia";
    CHECK(m.size() == 4 + sizeof body - 1);
    CHECK(m[0] == 0 && m[1] == 0 && m[2] == 0 && m[3] == sizeof body - 1);
    CHECK(std::memcmp(m.data() + 4, body, sizeof body - 1) == 0);
}

NNH_TEST(http_urls_encode_the_file_name) {
    CHECK(http_url("192.168.10.53", 8090, "audio.cia") == "http://192.168.10.53:8090/audio.cia");
    CHECK(http_url("10.0.0.1", 80, "my game+1.cia") == "http://10.0.0.1:80/my%20game%2B1.cia");
}
