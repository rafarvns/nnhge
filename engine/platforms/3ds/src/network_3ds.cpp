// Nintendo 3DS platform: the socket service, shared (reference-counted) by the game (<nnh/3ds/network.hpp>)
// and the Development log redirect (app_3ds.cpp).

#include <3ds.h>
#include <malloc.h>

#include <cstdlib>

#include "context_3ds.hpp"
#include "nnh/3ds/network.hpp"

namespace nnh {
namespace {

constexpr std::size_t kSocAlign = 0x1000;
constexpr std::size_t kSocBufferSize = 0x100000; // 1 MB

u32* g_buffer = nullptr;
int g_users = 0;

void release() {
    if (g_buffer == nullptr) return;
    socExit();
    std::free(g_buffer);
    g_buffer = nullptr;
}

} // namespace

bool n3ds::network_start() {
    if (g_users > 0) {
        ++g_users;
        return true;
    }
    g_buffer = static_cast<u32*>(memalign(kSocAlign, kSocBufferSize));
    if (g_buffer == nullptr) return false;
    if (R_FAILED(socInit(g_buffer, kSocBufferSize))) {
        std::free(g_buffer);
        g_buffer = nullptr;
        return false;
    }
    g_users = 1;
    return true;
}

void n3ds::network_stop() {
    if (g_users == 0) return;
    if (--g_users == 0) release();
}

void platform::ctr::network_release_all() {
    g_users = 0;
    release();
}

} // namespace nnh
