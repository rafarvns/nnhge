// Writing the received game and starting it (device.hpp). The game is streamed with stdio, not nnh::fs:
// nnh::fs writes whole files only, and a game must not be held in RAM (constitution II). The name is
// checked (valid_game_name: letters, digits, '_', '-') and the folder is Link's data folder.

#include "device.hpp"

#include <cstring>

#include "nnh_link/messages.hpp"
#include "platform_link.hpp"

namespace link_app {

namespace {

constexpr std::uint64_t kSpareBytes = 1024 * 1024; // keep 1 MB free on the SD card
constexpr char kDefaultSelf[] = "sdmc:/3ds/nnh-link.3dsx";

} // namespace

std::string return_argument(const std::string& self) { return "--nnh-return=" + self; }

std::string log_host_argument(nnh::link::Ipv4 server) {
    // libctru parses the hex into in_addr::s_addr, whose first byte in memory is the first octet.
    const std::uint32_t s_addr = static_cast<std::uint32_t>(server.b[0]) | static_cast<std::uint32_t>(server.b[1]) << 8 |
                                 static_cast<std::uint32_t>(server.b[2]) << 16 | static_cast<std::uint32_t>(server.b[3]) << 24;
    char text[24];
    std::snprintf(text, sizeof text, "%08lX_3DSLINK_", static_cast<unsigned long>(s_addr));
    return text;
}

std::string self_path(const char* argv0) {
    if (argv0 == nullptr || std::strncmp(argv0, "sdmc:/", 6) != 0) return kDefaultSelf;
    const std::size_t n = std::strlen(argv0);
    if (n < 6 || std::strcmp(argv0 + n - 5, ".3dsx") != 0) return kDefaultSelf;
    return argv0;
}

SdFileSink::~SdFileSink() { finish(false); }

void SdFileSink::close() {
    if (file_ != nullptr) std::fclose(file_);
    file_ = nullptr;
}

bool SdFileSink::begin(std::uint64_t size) {
    close();
    const std::size_t slash = path_.rfind('/');
    const std::uint64_t free = platform::free_space(slash == std::string::npos ? "." : path_.substr(0, slash));
    if (free != UINT64_MAX && (free < kSpareBytes || free - kSpareBytes < size)) return false;
    file_ = std::fopen((path_ + ".part").c_str(), "wb");
    return file_ != nullptr;
}

bool SdFileSink::write(const std::uint8_t* data, std::size_t size) {
    return file_ != nullptr && std::fwrite(data, 1, size, file_) == size;
}

void SdFileSink::finish(bool ok) {
    if (file_ == nullptr) return;
    const bool closed = std::fclose(file_) == 0;
    file_ = nullptr;
    const std::string part = path_ + ".part";
    if (ok && closed) {
        std::remove(path_.c_str()); // rename doesn't replace on every system
        if (std::rename(part.c_str(), path_.c_str()) == 0) return;
    }
    std::remove(part.c_str());
}

nnh::link::FileSink& SdDevice::sink_for(const std::string& name) {
    // ClientSession only builds valid names; this keeps the path inside the folder whatever happens.
    sink_.set_path(nnh::link::valid_game_name(name) ? path_of(name) : path_of("invalid"));
    return sink_;
}

bool SdDevice::launch(const std::string& name, nnh::link::Ipv4 server) {
    if (!nnh::link::valid_game_name(name)) return false;
    const std::string back = return_argument(self_);
    const std::string log = log_host_argument(server);
    const char* args[] = {back.c_str(), log.c_str()};
    return platform::launch_after_exit(path_of(name), args, 2);
}

} // namespace link_app
