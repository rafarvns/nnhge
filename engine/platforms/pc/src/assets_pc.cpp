// PC simulator: the game's assets in the `assets` folder next to the executable, where the build puts
// them (contract nnh/platform/assets.hpp, spec 007). Core has already validated every path and checked
// it against the index (case included), so this file only opens and reads.

#include <SDL3/SDL.h>

#include <cstdio>

#include "nnh/platform/assets.hpp"

namespace nnh::platform {
namespace {

char g_root[512] = ""; // "<exe dir>assets/" while mounted

bool full_path(const char* path, char* out, std::size_t size) {
    const int n = std::snprintf(out, size, "%s%s", g_root, path);
    return n > 0 && static_cast<std::size_t>(n) < size;
}

} // namespace

bool assets_mount() {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) return false;
    std::snprintf(g_root, sizeof g_root, "%sassets/", base);
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(g_root, &info) || info.type != SDL_PATHTYPE_DIRECTORY) {
        g_root[0] = '\0';
        return false;
    }
    return true;
}

void assets_unmount() { g_root[0] = '\0'; }

bool asset_size(const char* path, std::size_t& out) {
    char full[1024];
    SDL_PathInfo info;
    if (g_root[0] == '\0' || !full_path(path, full, sizeof full) || !SDL_GetPathInfo(full, &info) ||
        info.type != SDL_PATHTYPE_FILE) {
        return false;
    }
    out = static_cast<std::size_t>(info.size);
    return true;
}

bool asset_read(const char* path, void* buffer, std::size_t capacity, std::size_t& read) {
    read = 0;
    char full[1024];
    if (g_root[0] == '\0' || !full_path(path, full, sizeof full)) return false;
    SDL_IOStream* io = SDL_IOFromFile(full, "rb");
    if (io == nullptr) return false;
    const Sint64 size = SDL_GetIOSize(io);
    bool ok = size >= 0 && static_cast<std::size_t>(size) <= capacity;
    if (ok && size > 0) {
        read = SDL_ReadIO(io, buffer, static_cast<std::size_t>(size));
        ok = read == static_cast<std::size_t>(size);
    }
    SDL_CloseIO(io);
    return ok;
}

bool asset_open(const char* path, AssetStream& out) {
    out = 0;
    char full[1024];
    if (g_root[0] == '\0' || !full_path(path, full, sizeof full)) return false;
    SDL_IOStream* io = SDL_IOFromFile(full, "rb");
    out = reinterpret_cast<AssetStream>(io);
    return io != nullptr;
}

bool asset_read_at(AssetStream stream, std::uint64_t offset, void* buffer, std::size_t size, std::size_t& read) {
    read = 0;
    auto* io = reinterpret_cast<SDL_IOStream*>(stream);
    if (io == nullptr || SDL_SeekIO(io, static_cast<Sint64>(offset), SDL_IO_SEEK_SET) < 0) return false;
    read = size > 0 ? SDL_ReadIO(io, buffer, size) : 0;
    return read == size || SDL_GetIOStatus(io) == SDL_IO_STATUS_EOF;
}

void asset_close(AssetStream stream) {
    if (stream != 0) SDL_CloseIO(reinterpret_cast<SDL_IOStream*>(stream));
}

} // namespace nnh::platform
