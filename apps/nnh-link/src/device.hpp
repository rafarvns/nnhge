#pragma once
// The real Device (ports.hpp): the received game is written to Link's data folder as it arrives (a
// ".part" file renamed when complete and verified), then started with Link's return and log arguments.

#include <cstdio>
#include <string>

#include "nnh_link/address.hpp"
#include "ports.hpp"

namespace link_app {

/// "--nnh-return=<self>": the engine starts Link again when the game exits (<nnh/3ds/launch.hpp>).
std::string return_argument(const std::string& self);
/// "<address in hex>_3DSLINK_": the game sends its log to `server`, like after the netloader. libctru
/// reads it only as the last argument.
std::string log_host_argument(nnh::link::Ipv4 server);
/// Link's own .3dsx (argv[0]) when it is a path on the SD card, else the usual "sdmc:/3ds/nnh-link.3dsx".
std::string self_path(const char* argv0);

/// Streams the game to `<path>.part`, checks the free space first, renames on success.
class SdFileSink : public nnh::link::FileSink {
public:
    ~SdFileSink() override;
    void set_path(const std::string& path) { path_ = path; }
    bool begin(std::uint64_t size) override;
    bool write(const std::uint8_t* data, std::size_t size) override;
    void finish(bool ok) override;

private:
    void close();
    std::string path_;
    std::FILE* file_ = nullptr;
};

class SdDevice : public Device {
public:
    /// `folder`: where games go (Link's data folder, ending in '/'); `self`: Link's .3dsx.
    SdDevice(std::string folder, std::string self) : folder_(static_cast<std::string&&>(folder)), self_(static_cast<std::string&&>(self)) {}

    nnh::link::FileSink& sink_for(const std::string& name) override;
    std::string path_of(const std::string& name) override { return folder_ + name + ".3dsx"; }
    bool launch(const std::string& name, nnh::link::Ipv4 server) override;

private:
    std::string folder_;
    std::string self_;
    SdFileSink sink_;
};

} // namespace link_app
