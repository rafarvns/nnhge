// nnh::fs file operations: validate, then call the platform contract.

#include <cstdio>

#include "core/engine_state.hpp"
#include "nnh/fs.hpp"
#include "nnh/platform/fs.hpp"

namespace nnh::fs {
namespace {

/// Validates the path and that the engine is running; fills `error` when not.
Result<void> check(const char* path) {
    if (!detail::engine_initialized()) return Error::make(ErrorCode::Unsupported, "nnh::init() not called");
    return validate_path(path);
}

/// "<path><suffix>" into `out`; fails if it doesn't fit.
bool with_suffix(char (&out)[kMaxPathLength + 8], const char* path, const char* suffix) {
    const int n = std::snprintf(out, sizeof out, "%s%s", path, suffix);
    return n > 0 && static_cast<std::size_t>(n) < sizeof out;
}

} // namespace

const char* data_root() { return platform::fs_root(); }

Result<std::size_t> read_file(const char* path, void* buffer, std::size_t capacity) {
    if (auto ok = check(path); !ok) return ok.error();
    const std::int64_t size = platform::fs_size(path);
    if (size < 0) return Error::make(ErrorCode::NotFound, "%s not found", path);
    if (static_cast<std::uint64_t>(size) > capacity) {
        return Error::make(ErrorCode::LimitReached, "%s is %lld bytes, buffer %zu", path,
                           static_cast<long long>(size), capacity);
    }
    const std::int64_t read = platform::fs_read(path, 0, buffer, static_cast<std::size_t>(size));
    if (read != size) return Error::make(ErrorCode::IoError, "reading %s failed", path);
    return static_cast<std::size_t>(read);
}

Result<void> write_file(const char* path, const void* data, std::size_t size) {
    if (auto ok = check(path); !ok) return ok;
    const platform::FsChunk chunk{data, size};
    if (!platform::fs_write(path, &chunk, 1)) return Error::make(ErrorCode::IoError, "writing %s failed", path);
    return {};
}

Result<void> write_file_atomic(const char* path, const void* data, std::size_t size) {
    if (auto ok = check(path); !ok) return ok;
    char tmp[kMaxPathLength + 8];
    char bak[kMaxPathLength + 8];
    if (!with_suffix(tmp, path, ".tmp") || !with_suffix(bak, path, ".bak")) {
        return Error::make(ErrorCode::InvalidArgument, "path too long for atomic write");
    }
    const platform::FsChunk chunk{data, size};
    if (!platform::fs_write(tmp, &chunk, 1)) {
        platform::fs_remove(tmp);
        return Error::make(ErrorCode::IoError, "writing %s failed", tmp);
    }
    if (platform::fs_size(path) >= 0) {
        platform::fs_remove(bak);
        if (!platform::fs_rename(path, bak)) return Error::make(ErrorCode::IoError, "backup of %s failed", path);
    }
    if (!platform::fs_rename(tmp, path)) return Error::make(ErrorCode::IoError, "replacing %s failed", path);
    return {};
}

Result<std::size_t> file_size(const char* path) {
    if (auto ok = check(path); !ok) return ok.error();
    const std::int64_t size = platform::fs_size(path);
    if (size < 0) return Error::make(ErrorCode::NotFound, "%s not found", path);
    return static_cast<std::size_t>(size);
}

bool exists(const char* path) { return check(path).ok() && platform::fs_size(path) >= 0; }

Result<void> remove(const char* path) {
    if (auto ok = check(path); !ok) return ok;
    if (!platform::fs_remove(path)) return Error::make(ErrorCode::IoError, "removing %s failed", path);
    return {};
}

} // namespace nnh::fs
