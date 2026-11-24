// File helpers of the saves example (slot_files.hpp): only public engine calls.

#include "slot_files.hpp"

#include <cstdio>
#include <nnh/assets.hpp>
#include <nnh/fs.hpp>
#include <nnh/saves.hpp>

namespace saves_example {

const char* slot_path(int slot) {
    static constexpr const char* kPaths[kSlots] = {"slot1.sav", "slot2.sav", "slot3.sav"};
    return slot >= 0 && slot < kSlots ? kPaths[slot] : "slot1.sav";
}

bool damage_save(const char* path, std::uint8_t* scratch, std::size_t capacity) {
    // The payload (spec 002's read checks the CRC), with the first field's wire byte made invalid (0x7F)...
    const auto payload = nnh::save::read(path, scratch, capacity, nullptr);
    if (!payload || payload.value() < 24) return false;
    const std::size_t first_field = 16 + scratch[5]; // the structured header: 16 bytes, then the key
    if (first_field >= payload.value()) return false;
    scratch[first_field] = 0x7F;
    // ...written as a whole file with a valid CRC (through a scratch save), then put in place of the main
    // file only: its backup stays as it was.
    constexpr const char* kScratch = "damage_scratch.sav";
    if (!nnh::save::write(kScratch, scratch, payload.value(), 1)) return false;
    const auto file = nnh::fs::read_file(kScratch, scratch, capacity);
    (void)nnh::save::remove(kScratch);
    return file.ok() && nnh::fs::write_file(path, scratch, file.value()).ok();
}

const char* install_golden(int version, std::uint8_t* scratch, std::size_t capacity) {
    static char path[24];
    char asset[24];
    std::snprintf(asset, sizeof asset, "golden/v%d.sav", version);
    std::snprintf(path, sizeof path, "golden_v%d.sav", version);
    const auto n = nnh::assets::read(asset, scratch, capacity);
    if (!n || !nnh::fs::write_file(path, scratch, n.value())) return nullptr;
    return path;
}

std::uint32_t crc32_of(const void* data, std::size_t size) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= p[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

} // namespace saves_example
