#pragma once
// File helpers of the saves example (spec 027): slot paths, damaging a save on purpose (to show the backup),
// the golden files copied from the assets to the data folder, and a CRC-32 (the check mode's encode line).

#include <cstddef>
#include <cstdint>

namespace saves_example {

inline constexpr int kSlots = 3;

/// "slot1.sav".."slot3.sav" (slot 0..2).
const char* slot_path(int slot);

/// Rewrites the main file of `path` with a valid CRC over a broken structure (its first field's wire byte),
/// keeping its backup: the next load uses the backup. `scratch` holds a whole file. Returns whether it did.
bool damage_save(const char* path, std::uint8_t* scratch, std::size_t capacity);

/// Copies the asset golden/v<version>.sav to golden_v<version>.sav in the data folder (the SD card on the
/// 3DS). Returns the path written, or nullptr.
const char* install_golden(int version, std::uint8_t* scratch, std::size_t capacity);

/// CRC-32 (IEEE, as spec 002's container uses).
std::uint32_t crc32_of(const void* data, std::size_t size);

} // namespace saves_example
