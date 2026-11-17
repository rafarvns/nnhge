// fs contract on C stdio (see stdio_fs.hpp). Paths arrive validated and relative to the data root.

#include "stdio_fs.hpp"

#include <cstdio>
#include <cstring>

#include "nnh/platform/fs.hpp"

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nnh::platform {
namespace {

char g_root[256] = "";

/// root + relative path into `out`.
bool full_path(char (&out)[512], const char* path) {
    const int n = std::snprintf(out, sizeof out, "%s%s", g_root, path);
    return n > 0 && static_cast<std::size_t>(n) < sizeof out;
}

void make_dir(const char* path) {
#if defined(_WIN32)
    _mkdir(path);
#else
    mkdir(path, 0777);
#endif
}

/// Creates every folder of `path` (absolute) up to its last '/'. Existing folders are fine.
void make_parents(const char* path) {
    char partial[512];
    for (std::size_t i = 0; path[i] != '\0' && i < sizeof partial - 1; ++i) {
        partial[i] = path[i];
        if ((path[i] == '/' || path[i] == '\\') && i > 0 && path[i - 1] != ':') {
            partial[i] = '\0';
            make_dir(partial);
            partial[i] = path[i];
        }
    }
}

void flush_to_storage(std::FILE* file) {
    std::fflush(file);
#if defined(_WIN32)
    _commit(_fileno(file));
#elif !defined(__3DS__)
    fsync(fileno(file));
#endif // 3DS: fclose() commits the file through the FS service
}

} // namespace

bool shared::stdio_fs_set_root(const char* root) {
    const int n = std::snprintf(g_root, sizeof g_root, "%s", root);
    if (n <= 0 || static_cast<std::size_t>(n) >= sizeof g_root) return false;
    make_parents(g_root);
    return true;
}

const char* fs_root() { return g_root; }

std::int64_t fs_size(const char* path) {
    char full[512];
    if (!full_path(full, path)) return -1;
    std::FILE* file = std::fopen(full, "rb");
    if (file == nullptr) return -1;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fclose(file);
    return size < 0 ? -1 : static_cast<std::int64_t>(size);
}

std::int64_t fs_read(const char* path, std::uint64_t offset, void* buffer, std::size_t capacity) {
    char full[512];
    if (!full_path(full, path)) return -1;
    std::FILE* file = std::fopen(full, "rb");
    if (file == nullptr) return -1;
    std::int64_t result = -1;
    if (std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0) {
        result = static_cast<std::int64_t>(std::fread(buffer, 1, capacity, file));
    }
    std::fclose(file);
    return result;
}

bool fs_write(const char* path, const FsChunk* chunks, int count) {
    char full[512];
    if (!full_path(full, path)) return false;
    make_parents(full);
    std::FILE* file = std::fopen(full, "wb");
    if (file == nullptr) return false;
    bool ok = true;
    for (int i = 0; i < count && ok; ++i) {
        ok = chunks[i].size == 0 || std::fwrite(chunks[i].data, 1, chunks[i].size, file) == chunks[i].size;
    }
    flush_to_storage(file);
    return std::fclose(file) == 0 && ok;
}

bool fs_append(const char* path, const void* data, std::size_t size) {
    char full[512];
    if (!full_path(full, path)) return false;
    std::FILE* file = std::fopen(full, "ab");
    if (file == nullptr) return false;
    const bool ok = std::fwrite(data, 1, size, file) == size;
    return std::fclose(file) == 0 && ok;
}

bool fs_rename(const char* from, const char* to) {
    char full_from[512];
    char full_to[512];
    if (!full_path(full_from, from) || !full_path(full_to, to)) return false;
    if (fs_size(to) >= 0) return false; // contract: never replace silently
    return std::rename(full_from, full_to) == 0;
}

bool fs_remove(const char* path) {
    char full[512];
    if (!full_path(full, path)) return false;
    return std::remove(full) == 0 || fs_size(path) < 0;
}

} // namespace nnh::platform
