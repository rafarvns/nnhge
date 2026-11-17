// Save file format helpers (see save_format.hpp).

#include "fs/save_format.hpp"

namespace nnh::detail {
namespace {

std::uint32_t read_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
           static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}

void write_u32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}

} // namespace

std::uint32_t crc32(const void* data, std::size_t size) {
    // Standard CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320), bitwise: small and table-free.
    std::uint32_t crc = 0xFFFFFFFFu;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

std::size_t build_save_header(std::uint8_t* out, std::uint32_t payload_size, std::uint16_t version,
                              std::uint32_t crc) {
    write_u32(out, kSaveMagic);
    out[4] = static_cast<std::uint8_t>(version);
    out[5] = static_cast<std::uint8_t>(version >> 8);
    out[6] = out[7] = 0;
    write_u32(out + 8, payload_size);
    write_u32(out + 12, crc);
    return kSaveHeaderSize;
}

Result<SaveHeader> parse_save_header(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < kSaveHeaderSize) return Error::make(ErrorCode::Corrupted, "save too short");
    if (read_u32(data) != kSaveMagic) return Error::make(ErrorCode::Corrupted, "not a save file");
    SaveHeader header{static_cast<std::uint16_t>(data[4] | data[5] << 8), read_u32(data + 8), read_u32(data + 12)};
    if (header.size > kMaxSavePayload) return Error::make(ErrorCode::Corrupted, "save size out of range");
    return header;
}

Result<SaveView> validate_save(const std::uint8_t* data, std::size_t size) {
    auto header = parse_save_header(data, size);
    if (!header) return header.error();
    const SaveHeader h = header.value();
    if (size != kSaveHeaderSize + h.size) return Error::make(ErrorCode::Corrupted, "save length mismatch");
    const std::uint8_t* payload = data + kSaveHeaderSize;
    if (crc32(payload, h.size) != h.crc) return Error::make(ErrorCode::Corrupted, "save checksum mismatch");
    return SaveView{payload, h.size, h.version};
}

} // namespace nnh::detail
