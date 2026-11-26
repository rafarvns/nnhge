// Nintendo 3DS: hot reload's two stream opens (contract nnh/platform/reload.hpp, spec 031). Debug/Development only:
// core reaches these from discarded branches in Release. A received file is read from the game's data folder on the SD
// card; the romfs is read-only, so a "shared" open of a packed asset is the plain one.

#include <cstdio>

#include "nnh/platform/fs.hpp"
#include "nnh/platform/reload.hpp"

namespace nnh::platform {

bool reload_stream_open(const char* data_path, AssetStream& out) {
    char full[320];
    const int n = std::snprintf(full, sizeof full, "%s%s", fs_root(), data_path);
    out = 0;
    if (n <= 0 || static_cast<std::size_t>(n) >= sizeof full) return false;
    std::FILE* file = std::fopen(full, "rb");
    out = reinterpret_cast<AssetStream>(file);
    return file != nullptr;
}

bool asset_open_shared(const char* path, AssetStream& out) { return asset_open(path, out); }

} // namespace nnh::platform
