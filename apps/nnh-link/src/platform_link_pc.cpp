// NNH Link in the PC simulator: everything but launching (spec 011 FR-6).

#include <filesystem>
#include <random>

#include "nnh_link/net.hpp"
#include "platform_link.hpp"

namespace link_app::platform {

bool start_network() { return nnh::link::net::startup(); }
void stop_network() {}

void fill_random(std::uint8_t* out, std::size_t size) {
    std::random_device device;
    for (std::size_t i = 0; i < size; ++i) out[i] = static_cast<std::uint8_t>(device());
}

std::uint64_t free_space(const std::string& folder) {
    std::error_code ec;
    const auto info = std::filesystem::space(folder, ec);
    return ec ? UINT64_MAX : info.available;
}

bool launch_after_exit(const std::string&, const char* const*, int) { return false; }

} // namespace link_app::platform
