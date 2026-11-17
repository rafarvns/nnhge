#pragma once
/// @file nnh/fs.hpp
/// Safe file access inside the game's data folder, and crash-safe save files.
/// See docs/api/fs.md and docs/architecture/security.md.
///
/// @code
/// struct Progress { int level; int coins; };
/// Progress p{3, 120};
/// nnh::save::write("slot1.sav", &p, sizeof p, /*version*/ 1);
/// if (auto r = nnh::save::read("slot1.sav", &p, sizeof p); !r) start_new_game();
/// @endcode
///
/// Every path is relative to the data folder (3DS: sdmc:/3ds/<data_folder>/) and validated: no "..",
/// no absolute paths, no device prefixes, only [A-Za-z0-9._-] and '/'. Nothing outside the folder can
/// be read or written through this API. Requires nnh::init().

#include <cstddef>
#include <cstdint>

#include "nnh/result.hpp"

namespace nnh::fs {

inline constexpr std::size_t kMaxPathLength = 96; ///< characters, excluding '\0'
inline constexpr int kMaxPathDepth = 4;           ///< folders + file name

/// Checks that `path` is a safe relative path (see above). Pure; no file access.
Result<void> validate_path(const char* path);

/// Absolute location of the data folder (for messages), e.g. "sdmc:/3ds/my-game/".
const char* data_root();

/// Reads a whole file into `buffer`. @return bytes read. Errors: InvalidArgument (path), NotFound,
/// LimitReached (file larger than `capacity`; nothing is written past `capacity`), IoError.
Result<std::size_t> read_file(const char* path, void* buffer, std::size_t capacity);

/// Creates or replaces a file. Not crash-safe: use write_file_atomic() or nnh::save for important data.
Result<void> write_file(const char* path, const void* data, std::size_t size);

/// Replaces a file so that a power cut at any moment leaves either the old or the new content:
/// writes `<path>.tmp`, keeps the old file as `<path>.bak`, then renames the temp file into place.
Result<void> write_file_atomic(const char* path, const void* data, std::size_t size);

Result<std::size_t> file_size(const char* path);
bool exists(const char* path);
Result<void> remove(const char* path);

} // namespace nnh::fs

namespace nnh::save {

/// Writes a save file atomically with a header (magic, format version, size, CRC-32).
Result<void> write(const char* path, const void* data, std::size_t size, std::uint16_t version);

/// Reads and validates a save file into `buffer`. If the file is missing or corrupted, falls back to the
/// backup kept by the previous write. @return payload size; `version_out` (optional) gets the version.
/// Errors: NotFound (no save yet), Corrupted (file and backup both invalid), LimitReached, IoError.
Result<std::size_t> read(const char* path, void* buffer, std::size_t capacity, std::uint16_t* version_out);

} // namespace nnh::save
