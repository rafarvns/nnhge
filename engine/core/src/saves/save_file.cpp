// Structured saves as files (spec 027, D46, FR-6, FR-7, FR-9, FR-16): store through spec 002's unchanged
// nnh::save::write, load through its own read of one file with the backup chosen here (a CRC-valid file whose
// structure is broken is damage too), and the slot helpers.

#include "fs/save_format.hpp"
#include "nnh/fs.hpp"
#include "nnh/platform/fs.hpp"
#include "saves/save_header.hpp"
#include "saves/save_internal.hpp"
#include "saves/save_wire.hpp"

#include <cstdio>
#include <cstring>

namespace nnh::detail {
namespace {

using Name = char[fs::kMaxPathLength + 8];

bool with_suffix(Name& out, const char* path, const char* suffix) {
    std::snprintf(out, sizeof out, "%s%s", path, suffix);
    return fs::validate_path(out).ok();
}

Error named(const char* file, const Error& e) {
    return Error::make(e.code, "%s: %s", file, e.message);
}

/// Reads and decodes one file. `damage`: the error is one the backup may cure (missing, unreadable,
/// corrupted).
Result<save::Report> load_one(const SaveTypeInfo& type, const char* file, void* value, void* buffer,
                              std::size_t capacity, bool& damage) {
    auto read = save_read_one(file, buffer, capacity, nullptr);
    if (!read) {
        damage = read.error().code != ErrorCode::LimitReached;
        return read.error(); // spec 002's messages already name the file
    }
    auto decoded = save_decode_payload(type, static_cast<const std::uint8_t*>(buffer), read.value(), value);
    if (!decoded) {
        damage = decoded.error().code == ErrorCode::Corrupted;
        return named(file, decoded.error());
    }
    return decoded;
}

/// The two headers at the start of `file`: one size and one read of 16 + 32 bytes. `damage` as load_one's.
Result<save::FileInfo> info_one(const char* file, bool& damage) {
    damage = true;
    const std::int64_t size = platform::fs_size(file);
    if (size < 0) return Error::make(ErrorCode::NotFound, "%s not found", file);
    std::uint8_t bytes[kSaveHeaderSize + kSaveStructHeaderSize + save::kMaxKeyLength];
    const std::int64_t want = size < static_cast<std::int64_t>(sizeof bytes) ? size : sizeof bytes;
    if (platform::fs_read(file, 0, bytes, static_cast<std::size_t>(want)) != want) {
        return Error::make(ErrorCode::IoError, "reading %s failed", file);
    }
    auto outer = parse_save_header(bytes, static_cast<std::size_t>(want));
    if (!outer) return named(file, outer.error());
    if (static_cast<std::uint64_t>(size) != kSaveHeaderSize + outer.value().size) {
        return Error::make(ErrorCode::Corrupted, "%s length mismatch", file);
    }
    const std::size_t payload = static_cast<std::size_t>(want) - kSaveHeaderSize;
    auto inner = parse_save_struct_header(bytes + kSaveHeaderSize,
                                          payload < outer.value().size ? payload : outer.value().size);
    if (!inner) {
        damage = inner.error().code == ErrorCode::Corrupted;
        return named(file, inner.error());
    }
    save::FileInfo info;
    info.key_hash = inner.value().key_hash;
    std::memcpy(info.key, inner.value().key, sizeof info.key);
    info.version = inner.value().version;
    info.size = outer.value().size;
    return info;
}

} // namespace

Result<void> save_store(const SaveTypeInfo& type, const char* path, const void* value, void* buffer,
                        std::size_t capacity) {
    if (auto ok = fs::validate_path(path); !ok) return ok;
    if (buffer == nullptr) return Error::make(ErrorCode::InvalidArgument, "no buffer");
    auto encoded = save_encode(type, value, buffer, capacity);
    if (!encoded) return encoded.error();
    if (auto written = save::write(path, buffer, encoded.value(), save::kFormatVersion); !written)
        return written;
    save_dump_on_store(type, path, static_cast<const std::uint8_t*>(buffer), encoded.value());
    return {};
}

Result<save::Report> save_load(const SaveTypeInfo& type, const char* path, void* value, void* buffer,
                               std::size_t capacity) {
    if (auto ok = fs::validate_path(path); !ok) return ok.error();
    Name backup;
    if (buffer == nullptr || !with_suffix(backup, path, ".bak")) {
        return Error::make(ErrorCode::InvalidArgument,
                           buffer == nullptr ? "no buffer" : "save path too long");
    }
    bool damage = false;
    auto main = load_one(type, path, value, buffer, capacity, damage);
    if (main) {
        save_log_report(path, type, main.value(), nullptr);
        return main;
    }
    if (!damage) return main; // another type, a newer version or format, a raw file, a buffer too small
    bool backup_damage = false;
    auto old = load_one(type, backup, value, buffer, capacity, backup_damage);
    if (old) {
        old.value().used_backup = true;
        save_log_report(path, type, old.value(), main.error().message);
        return old;
    }
    if (!backup_damage) return old;
    if (main.error().code == ErrorCode::NotFound && old.error().code == ErrorCode::NotFound) {
        return Error::make(ErrorCode::NotFound, "%s not found", path);
    }
    NNH_LOG_WARN(saves, "save %s unusable (%s); backup unusable (%s)", path, main.error().message,
                 old.error().message);
    return Error::make(ErrorCode::Corrupted, "%s and its backup are unusable", path);
}

} // namespace nnh::detail

namespace nnh::save {

Result<FileInfo> info(const char* path) {
    if (auto ok = fs::validate_path(path); !ok) return ok.error();
    detail::Name backup;
    if (!detail::with_suffix(backup, path, ".bak"))
        return Error::make(ErrorCode::InvalidArgument, "save path too long");
    bool damage = false;
    auto main = detail::info_one(path, damage);
    if (main || !damage) return main;
    bool backup_damage = false;
    auto old = detail::info_one(backup, backup_damage);
    if (!old) return main.error().code == ErrorCode::NotFound ? old : main;
    old.value().from_backup = true;
    return old;
}

bool exists(const char* path) {
    detail::Name backup;
    if (!fs::validate_path(path) || !detail::with_suffix(backup, path, ".bak")) return false;
    return platform::fs_size(path) >= 0 || platform::fs_size(backup) >= 0;
}

Result<void> remove(const char* path) {
    if (auto ok = fs::validate_path(path); !ok) return ok;
    detail::Name backup;
    detail::Name temp;
    if (!detail::with_suffix(backup, path, ".bak") || !detail::with_suffix(temp, path, ".tmp")) {
        return Error::make(ErrorCode::InvalidArgument, "save path too long");
    }
    const char* files[3] = {path, backup, temp};
    for (const char* file : files) {
        if (platform::fs_size(file) >= 0) platform::fs_remove(file);
    }
    for (const char* file : files) {
        if (platform::fs_size(file) >= 0) return Error::make(ErrorCode::IoError, "removing %s failed", file);
    }
    return {};
}

} // namespace nnh::save
