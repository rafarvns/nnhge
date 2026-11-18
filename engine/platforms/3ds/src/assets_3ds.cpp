// Nintendo 3DS: the game's assets in the romfs packed into the .3dsx by the build (contract
// nnh/platform/assets.hpp, spec 007). Core has already validated every path and checked it against the
// index, so this file only opens and reads "romfs:/<path>".

#include <3ds.h>

#include <cstdio>

#include "nnh/platform/assets.hpp"

namespace nnh::platform {
namespace {

bool g_mounted = false;

bool full_path(const char* path, char* out, std::size_t size) {
    const int n = std::snprintf(out, size, "romfs:/%s", path);
    return n > 0 && static_cast<std::size_t>(n) < size;
}

} // namespace

bool assets_mount() {
    // A .3dsx without romfs (a game without ASSETS) fails here: it simply has no assets.
    g_mounted = R_SUCCEEDED(romfsInit());
    return g_mounted;
}

void assets_unmount() {
    if (g_mounted) romfsExit();
    g_mounted = false;
}

bool asset_size(const char* path, std::size_t& out) {
    char full[300];
    if (!g_mounted || !full_path(path, full, sizeof full)) return false;
    std::FILE* file = std::fopen(full, "rb");
    if (file == nullptr) return false;
    const bool ok = std::fseek(file, 0, SEEK_END) == 0;
    const long size = ok ? std::ftell(file) : -1;
    std::fclose(file);
    if (size < 0) return false;
    out = static_cast<std::size_t>(size);
    return true;
}

bool asset_read(const char* path, void* buffer, std::size_t capacity, std::size_t& read) {
    read = 0;
    std::size_t size = 0;
    char full[300];
    if (!asset_size(path, size) || size > capacity || !full_path(path, full, sizeof full)) return false;
    std::FILE* file = std::fopen(full, "rb");
    if (file == nullptr) return false;
    read = size > 0 ? std::fread(buffer, 1, size, file) : 0;
    std::fclose(file);
    return read == size;
}

bool asset_open(const char* path, AssetStream& out) {
    out = 0;
    char full[300];
    if (!g_mounted || !full_path(path, full, sizeof full)) return false;
    std::FILE* file = std::fopen(full, "rb");
    out = reinterpret_cast<AssetStream>(file);
    return file != nullptr;
}

bool asset_read_at(AssetStream stream, std::uint64_t offset, void* buffer, std::size_t size, std::size_t& read) {
    read = 0;
    auto* file = reinterpret_cast<std::FILE*>(stream);
    if (file == nullptr || std::fseek(file, static_cast<long>(offset), SEEK_SET) != 0) return false;
    read = size > 0 ? std::fread(buffer, 1, size, file) : 0;
    return read == size || std::feof(file) != 0;
}

void asset_close(AssetStream stream) {
    if (stream != 0) std::fclose(reinterpret_cast<std::FILE*>(stream));
}

} // namespace nnh::platform
