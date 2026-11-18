// Netloader messages (netloader.hpp): byte layout, the 16 KB chunk limit, arguments, names.

#include <cstring>

#include "netloader.hpp"
#include "test.hpp"

using namespace nnh::tool;

NNH_TEST(netloader_header_is_name_length_name_file_length) {
    const auto h = netloader_header("hello.3dsx", 0x01020304);
    const std::uint8_t expected[] = {10, 0, 0, 0, 'h', 'e', 'l', 'l', 'o', '.', '3', 'd', 's', 'x', 4, 3, 2, 1};
    CHECK(h.size() == sizeof expected && std::memcmp(h.data(), expected, sizeof expected) == 0);
}

NNH_TEST(netloader_chunks_never_exceed_16k) {
    std::vector<std::uint8_t> stream(kMaxNetloaderChunk * 2 + 5);
    for (std::size_t i = 0; i < stream.size(); ++i) stream[i] = static_cast<std::uint8_t>(i);
    const auto chunks = netloader_chunks(stream);
    CHECK(chunks.size() == 3);
    std::vector<std::uint8_t> joined;
    for (const auto& c : chunks) {
        const std::uint32_t size = c[0] | c[1] << 8 | c[2] << 16 | std::uint32_t(c[3]) << 24;
        CHECK(size == c.size() - 4 && size <= kMaxNetloaderChunk && size > 0);
        joined.insert(joined.end(), c.begin() + 4, c.end());
    }
    CHECK(joined == stream);
    CHECK(netloader_chunks({}).empty());
    CHECK(netloader_chunks(std::vector<std::uint8_t>(kMaxNetloaderChunk)).size() == 1);
}

NNH_TEST(netloader_args_are_nul_separated_with_a_length) {
    const auto a = netloader_args({"sdmc:/3ds/hello.3dsx", "-x"});
    const char body[] = "sdmc:/3ds/hello.3dsx\0-x"; // + the final NUL of the literal
    CHECK(a.size() == 4 + sizeof body);
    CHECK(a[0] == sizeof body && a[1] == 0 && a[2] == 0 && a[3] == 0);
    CHECK(std::memcmp(a.data() + 4, body, sizeof body) == 0);
    CHECK(netloader_args({}).size() == 4);
}

NNH_TEST(netloader_names_are_restricted) {
    CHECK(valid_netloader_name("hello.3dsx"));
    CHECK(valid_netloader_name("my_game-2.3dsx"));
    CHECK(!valid_netloader_name("../boot.3dsx"));
    CHECK(!valid_netloader_name("dir/game.3dsx"));
    CHECK(!valid_netloader_name("game.elf"));
    CHECK(!valid_netloader_name(".3dsx"));
    CHECK(!valid_netloader_name(std::string(201, 'a') + ".3dsx"));
    CHECK(netloader_path("hello.3dsx") == "sdmc:/3ds/hello.3dsx");
}

NNH_TEST(netloader_discovery_reply_is_recognised) {
    CHECK(is_discovery_reply("boot3ds", 7));
    CHECK(!is_discovery_reply("boot3d", 6));
    CHECK(!is_discovery_reply("3dsboot", 7));
}
