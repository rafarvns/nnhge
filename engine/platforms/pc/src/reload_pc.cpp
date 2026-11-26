// PC simulator: hot reload's two stream opens (contract nnh/platform/reload.hpp, spec 031). Debug/Development only:
// core reaches these from discarded branches in Release. A received file is read from the game's data folder; a packed
// asset streamed across frames (music) is opened so the build may rewrite it meanwhile: on Windows SDL's own file
// streams share reads only, so the file is opened with CreateFileW and every share flag behind an SDL_IOStream
// interface (asset_read_at/asset_close keep working); elsewhere the plain open already allows it.

#include <SDL3/SDL.h>

#include <cstdio>

#include "nnh/platform/fs.hpp"
#include "nnh/platform/reload.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace nnh::platform {
namespace {

#ifdef _WIN32
Sint64 SDLCALL win_size(void* userdata) {
    LARGE_INTEGER size;
    return GetFileSizeEx(static_cast<HANDLE>(userdata), &size) ? static_cast<Sint64>(size.QuadPart) : -1;
}

Sint64 SDLCALL win_seek(void* userdata, Sint64 offset, SDL_IOWhence whence) {
    const DWORD method = whence == SDL_IO_SEEK_SET ? FILE_BEGIN : whence == SDL_IO_SEEK_CUR ? FILE_CURRENT : FILE_END;
    LARGE_INTEGER to, now;
    to.QuadPart = offset;
    return SetFilePointerEx(static_cast<HANDLE>(userdata), to, &now, method) ? static_cast<Sint64>(now.QuadPart) : -1;
}

size_t SDLCALL win_read(void* userdata, void* ptr, size_t size, SDL_IOStatus* status) {
    DWORD got = 0;
    const DWORD want = size > 0x40000000u ? 0x40000000u : static_cast<DWORD>(size);
    if (!ReadFile(static_cast<HANDLE>(userdata), ptr, want, &got, nullptr)) {
        *status = SDL_IO_STATUS_ERROR;
        return 0;
    }
    if (got == 0 && size > 0) *status = SDL_IO_STATUS_EOF;
    return got;
}

size_t SDLCALL win_write(void*, const void*, size_t, SDL_IOStatus* status) {
    *status = SDL_IO_STATUS_READONLY;
    return 0;
}

bool SDLCALL win_close(void* userdata) { return CloseHandle(static_cast<HANDLE>(userdata)) != 0; }

SDL_IOStream* open_shared(const char* utf8) {
    wchar_t wide[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, 1024) == 0) return nullptr;
    HANDLE h = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return nullptr;
    SDL_IOStreamInterface iface;
    SDL_INIT_INTERFACE(&iface);
    iface.size = win_size;
    iface.seek = win_seek;
    iface.read = win_read;
    iface.write = win_write;
    iface.close = win_close;
    SDL_IOStream* io = SDL_OpenIO(&iface, h);
    if (io == nullptr) CloseHandle(h);
    return io;
}
#else
SDL_IOStream* open_shared(const char* path) { return SDL_IOFromFile(path, "rb"); }
#endif

} // namespace

bool reload_stream_open(const char* data_path, AssetStream& out) {
    char full[1024];
    const int n = std::snprintf(full, sizeof full, "%s%s", fs_root(), data_path);
    out = 0;
    if (n <= 0 || static_cast<std::size_t>(n) >= sizeof full) return false;
    SDL_IOStream* io = open_shared(full); // the game may remove a replaced file while a stream still reads it
    out = reinterpret_cast<AssetStream>(io);
    return io != nullptr;
}

bool asset_open_shared(const char* path, AssetStream& out) {
    out = 0;
    const char* base = SDL_GetBasePath(); // the folder assets_mount() found ("<exe dir>assets/")
    char full[1024];
    const int n = base != nullptr ? std::snprintf(full, sizeof full, "%sassets/%s", base, path) : -1;
    if (n <= 0 || static_cast<std::size_t>(n) >= sizeof full) return false;
    SDL_IOStream* io = open_shared(full);
    out = reinterpret_cast<AssetStream>(io);
    return io != nullptr;
}

} // namespace nnh::platform
