// Writing the received game and its launch arguments (apps/nnh-link/src/device.cpp, spec 011 FR-4, FR-5).

#include <cstdio>
#include <filesystem>
#include <string>

#include "device.hpp"
#include "test.hpp"

using namespace link_app;
using nnh::link::Ipv4;

namespace {

std::string temp_folder() {
    const auto dir = std::filesystem::temp_directory_path() / "nnh_link_app_tests";
    std::filesystem::create_directories(dir);
    return dir.generic_string() + "/";
}

std::string read_all(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return "<missing>";
    std::string text;
    char buffer[256];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) text.append(buffer, n);
    std::fclose(f);
    return text;
}

bool exists(const std::string& path) { return std::filesystem::exists(path); }

const std::uint8_t* bytes(const char* s) { return reinterpret_cast<const std::uint8_t*>(s); }

} // namespace

NNH_TEST(device_log_host_argument_is_s_addr_in_hex) {
    // 192.168.10.53: s_addr bytes c0 a8 0a 35 in memory = 0x350AA8C0 on the little-endian 3DS.
    CHECK(log_host_argument(Ipv4{{192, 168, 10, 53}}) == "350AA8C0_3DSLINK_");
    CHECK(log_host_argument(Ipv4{{10, 0, 0, 1}}) == "0100000A_3DSLINK_");
    CHECK(log_host_argument(Ipv4{{10, 0, 0, 1}}).size() == 17); // libctru wants exactly 17 characters
}

NNH_TEST(device_return_argument_names_link) {
    CHECK(return_argument("sdmc:/3ds/nnh-link.3dsx") == "--nnh-return=sdmc:/3ds/nnh-link.3dsx");
}

NNH_TEST(device_self_path_is_argv0_on_the_sd_card_or_the_usual_place) {
    CHECK(self_path("sdmc:/3ds/tools/nnh-link.3dsx") == "sdmc:/3ds/tools/nnh-link.3dsx");
    CHECK(self_path(nullptr) == "sdmc:/3ds/nnh-link.3dsx");
    CHECK(self_path("C:/x/nnh-link.exe") == "sdmc:/3ds/nnh-link.3dsx");
    CHECK(self_path("3dslink:/nnh-link.3dsx") == "sdmc:/3ds/nnh-link.3dsx");
    CHECK(self_path("sdmc:/3ds/notes.txt") == "sdmc:/3ds/nnh-link.3dsx");
}

NNH_TEST(device_sink_writes_a_complete_game_and_replaces_the_old_one) {
    SdDevice device(temp_folder(), "sdmc:/3ds/nnh-link.3dsx");
    const std::string path = device.path_of("sinktest");
    std::remove(path.c_str());
    for (const char* text : {"old", "new game"}) {
        nnh::link::FileSink& sink = device.sink_for("sinktest");
        CHECK(sink.begin(8));
        CHECK(sink.write(bytes(text), std::char_traits<char>::length(text)));
        sink.finish(true);
    }
    CHECK(read_all(path) == "new game");
    CHECK(!exists(path + ".part"));
}

NNH_TEST(device_sink_drops_a_failed_transfer_and_keeps_the_old_game) {
    SdDevice device(temp_folder(), "sdmc:/3ds/nnh-link.3dsx");
    const std::string path = device.path_of("sinkdrop");
    nnh::link::FileSink& sink = device.sink_for("sinkdrop");
    CHECK(sink.begin(3));
    CHECK(sink.write(bytes("old"), 3));
    sink.finish(true);
    CHECK(sink.begin(100));
    CHECK(sink.write(bytes("partial"), 7));
    CHECK(exists(path + ".part"));
    sink.finish(false);
    CHECK(!exists(path + ".part"));
    CHECK(read_all(path) == "old");
}

NNH_TEST(device_sink_refuses_a_game_larger_than_the_free_space) {
    SdDevice device(temp_folder(), "sdmc:/3ds/nnh-link.3dsx");
    nnh::link::FileSink& sink = device.sink_for("sinkhuge");
    CHECK(!sink.begin(UINT64_MAX / 2));
    CHECK(!exists(device.path_of("sinkhuge") + ".part"));
}

NNH_TEST(device_sink_keeps_an_invalid_name_inside_the_folder) {
    SdDevice device(temp_folder(), "sdmc:/3ds/nnh-link.3dsx");
    std::remove(device.path_of("invalid").c_str());
    nnh::link::FileSink& sink = device.sink_for("../escape");
    CHECK(sink.begin(1));
    CHECK(sink.write(bytes("x"), 1));
    sink.finish(true);
    CHECK(exists(device.path_of("invalid")));
    CHECK(!device.launch("../escape", Ipv4{{10, 0, 0, 1}}));
    CHECK(!device.launch("hello", Ipv4{{10, 0, 0, 1}})); // the PC can't launch
}
