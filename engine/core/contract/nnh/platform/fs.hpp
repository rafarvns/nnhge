#pragma once
/// @file nnh/platform/fs.hpp
/// Platform contract — files in the game's data folder. Implemented by every platform library, called
/// only by nnh::fs (core), which validates every path first. Not for games.
///
/// All paths are relative to the data folder chosen at init() (3DS: sdmc:/3ds/<folder>/; PC: a per-game
/// folder). They are already validated: no "..", no absolute paths, no device prefixes, '/' separators.
/// Results are plain values (no core types), so platform libraries keep not depending on core.

#include <cstddef>
#include <cstdint>

namespace nnh::platform {

struct FsChunk {
    const void* data;
    std::size_t size;
};

/// Absolute location of the data folder, for messages (e.g. "sdmc:/3ds/hello/"). Never null.
const char* fs_root();

/// Size of a file in bytes, or -1 if it doesn't exist.
std::int64_t fs_size(const char* path);

/// Reads up to `capacity` bytes starting at `offset`. @return bytes read, or -1 if the file can't be read.
std::int64_t fs_read(const char* path, std::uint64_t offset, void* buffer, std::size_t capacity);

/// Creates or replaces the file with the concatenated chunks, creating parent folders, and flushes it
/// to storage before returning. @return false on any failure (the file may then be partial).
bool fs_write(const char* path, const FsChunk* chunks, int count);

/// Appends bytes to a file, creating it if needed. Must not allocate per call beyond what the system does.
bool fs_append(const char* path, const void* data, std::size_t size);

/// Renames a file. Fails if `to` already exists (callers remove it first).
bool fs_rename(const char* from, const char* to);

/// Removes a file. @return true if it was removed or didn't exist.
bool fs_remove(const char* path);

} // namespace nnh::platform
