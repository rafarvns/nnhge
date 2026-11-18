// Assets (contract nnh/platform/assets.hpp): implemented in spec 007 stage 4. Until then: no assets.

#include "nnh/platform/assets.hpp"

namespace nnh::platform {

bool assets_mount() { return false; }
void assets_unmount() {}
bool asset_size(const char*, std::size_t&) { return false; }
bool asset_read(const char*, void*, std::size_t, std::size_t& read) {
    read = 0;
    return false;
}

} // namespace nnh::platform
