// nnh::save — atomic, checksummed save files with backup fallback.

#include <cstdio>

#include "fs/save_format.hpp"
#include "log/engine_channels.hpp"
#include "nnh/fs.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/fs.hpp"

namespace nnh::save {
namespace {

/// Reads and validates one file (header first, then the payload straight into `buffer`).
Result<std::size_t> read_one(const char* path, void* buffer, std::size_t capacity, std::uint16_t* version_out) {
    const std::int64_t file_size = platform::fs_size(path);
    if (file_size < 0) return Error::make(ErrorCode::NotFound, "%s not found", path);
    std::uint8_t header_bytes[detail::kSaveHeaderSize];
    if (platform::fs_read(path, 0, header_bytes, sizeof header_bytes) != static_cast<std::int64_t>(sizeof header_bytes)) {
        return Error::make(ErrorCode::Corrupted, "%s too short", path);
    }
    auto header = detail::parse_save_header(header_bytes, sizeof header_bytes);
    if (!header) return header.error();
    const detail::SaveHeader h = header.value();
    if (static_cast<std::uint64_t>(file_size) != detail::kSaveHeaderSize + h.size) {
        return Error::make(ErrorCode::Corrupted, "%s length mismatch", path);
    }
    if (h.size > capacity) return Error::make(ErrorCode::LimitReached, "%s needs %lu bytes", path, static_cast<unsigned long>(h.size));
    if (platform::fs_read(path, detail::kSaveHeaderSize, buffer, h.size) != static_cast<std::int64_t>(h.size)) {
        return Error::make(ErrorCode::IoError, "reading %s failed", path);
    }
    if (detail::crc32(buffer, h.size) != h.crc) return Error::make(ErrorCode::Corrupted, "%s checksum mismatch", path);
    if (version_out != nullptr) *version_out = h.version;
    return static_cast<std::size_t>(h.size);
}

} // namespace

Result<void> write(const char* path, const void* data, std::size_t size, std::uint16_t version) {
    if (auto ok = fs::validate_path(path); !ok) return ok;
    if (size > detail::kMaxSavePayload) return Error::make(ErrorCode::LimitReached, "save too large");
    std::uint8_t header[detail::kSaveHeaderSize];
    detail::build_save_header(header, static_cast<std::uint32_t>(size), version, detail::crc32(data, size));

    char tmp[fs::kMaxPathLength + 8];
    char bak[fs::kMaxPathLength + 8];
    std::snprintf(tmp, sizeof tmp, "%s.tmp", path);
    std::snprintf(bak, sizeof bak, "%s.bak", path);
    if (!fs::validate_path(tmp) || !fs::validate_path(bak)) {
        return Error::make(ErrorCode::InvalidArgument, "save path too long");
    }
    const platform::FsChunk chunks[2] = {{header, sizeof header}, {data, size}};
    if (!platform::fs_write(tmp, chunks, 2)) {
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

Result<std::size_t> read(const char* path, void* buffer, std::size_t capacity, std::uint16_t* version_out) {
    if (auto ok = fs::validate_path(path); !ok) return ok.error();
    auto main = read_one(path, buffer, capacity, version_out);
    if (main || main.error().code == ErrorCode::LimitReached) return main;

    char bak[fs::kMaxPathLength + 8];
    std::snprintf(bak, sizeof bak, "%s.bak", path);
    auto backup = read_one(bak, buffer, capacity, version_out);
    if (backup) {
        NNH_LOG_WARN(app, "save %s unusable (%s); loaded the backup", path, main.error().message);
        return backup;
    }
    if (main.error().code == ErrorCode::NotFound && backup.error().code == ErrorCode::NotFound) return main;
    return Error::make(ErrorCode::Corrupted, "%s and its backup are unusable", path);
}

} // namespace nnh::save
