// NNH Link on the 3DS: the network and launching through the engine's 3DS extensions (spec 011 FR-4).

#include <3ds.h>

#include <ctime>

#include <nnh/3ds/launch.hpp>
#include <nnh/3ds/network.hpp>

#include "platform_link.hpp"

namespace link_app::platform {

bool start_network() { return nnh::n3ds::network_start(); }
void stop_network() { nnh::n3ds::network_stop(); }

void fill_random(std::uint8_t* out, std::size_t size) {
    // The PS service's generator (the system's own). If it isn't available, mix the tick counter: the bytes
    // then only need to differ between connections (nonces) and consoles (the id).
    if (R_SUCCEEDED(psInit())) {
        const bool ok = R_SUCCEEDED(PS_GenerateRandomBytes(out, size));
        psExit();
        if (ok) return;
    }
    std::uint64_t x = svcGetSystemTick() ^ static_cast<std::uint64_t>(std::time(nullptr));
    for (std::size_t i = 0; i < size; ++i) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        out[i] = static_cast<std::uint8_t>(x >> 24);
    }
}

std::uint64_t free_space(const std::string&) {
    FS_ArchiveResource resource{};
    if (R_FAILED(FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_SD))) return UINT64_MAX;
    return static_cast<std::uint64_t>(resource.freeClusters) * resource.clusterSize;
}

bool launch_after_exit(const std::string& path, const char* const* args, int count) {
    return nnh::n3ds::can_launch() && nnh::n3ds::launch_after_exit(path.c_str(), args, count);
}

} // namespace link_app::platform
