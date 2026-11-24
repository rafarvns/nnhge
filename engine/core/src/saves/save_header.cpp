// The structured header of a structured save (save_header.hpp).

#include "saves/save_header.hpp"

#include "nnh/detail/name_hash.hpp"
#include "saves/save_wire.hpp"

#include <cstring>

namespace nnh::detail {

bool save_key_valid(const char* key) {
    if (key == nullptr || !(key[0] >= 'a' && key[0] <= 'z')) return false;
    int n = 0;
    for (; key[n] != '\0'; ++n) {
        const char c = key[n];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        if (!ok || n >= save::kMaxKeyLength) return false;
    }
    return true;
}

std::size_t build_save_struct_header(std::uint8_t* out, const char* key, std::uint32_t version) {
    const std::size_t length = std::strlen(key);
    store_u32(out, kSaveStructMagic);
    out[4] = static_cast<std::uint8_t>(save::kFormatVersion);
    out[5] = static_cast<std::uint8_t>(length);
    store_u16(out + 6, 0);
    store_u32(out + 8, version);
    store_u32(out + 12, fnv1a32(key, length));
    std::memcpy(out + kSaveStructHeaderSize, key, length);
    return kSaveStructHeaderSize + length;
}

Result<SaveStructHeader> parse_save_struct_header(const std::uint8_t* data, std::size_t size) {
    if (size < 4 || load_u32(data) != kSaveStructMagic) {
        return Error::make(ErrorCode::Unsupported, "a raw nnh::save file (read it with save::read)");
    }
    if (size < kSaveStructHeaderSize) return Error::make(ErrorCode::Corrupted, "structured header cut short");
    SaveStructHeader h;
    h.format = data[4];
    h.key_length = data[5];
    h.version = load_u32(data + 8);
    h.key_hash = load_u32(data + 12);
    if (h.format > save::kFormatVersion) {
        return Error::make(ErrorCode::Unsupported, "format %u; this engine reads %u",
                           static_cast<unsigned>(h.format), static_cast<unsigned>(save::kFormatVersion));
    }
    if (h.format == 0) return Error::make(ErrorCode::Corrupted, "format 0");
    if (h.key_length == 0 || h.key_length > save::kMaxKeyLength) {
        return Error::make(ErrorCode::Corrupted, "key length %u", static_cast<unsigned>(h.key_length));
    }
    if (size < kSaveStructHeaderSize + h.key_length) {
        return Error::make(ErrorCode::Corrupted, "structured header cut short");
    }
    if (load_u16(data + 6) != 0) return Error::make(ErrorCode::Corrupted, "unknown header flags");
    if (h.version == 0) return Error::make(ErrorCode::Corrupted, "save version 0");
    std::memcpy(h.key, data + kSaveStructHeaderSize, h.key_length);
    h.key[h.key_length] = '\0';
    if (std::strlen(h.key) != h.key_length || !save_key_valid(h.key))
        return Error::make(ErrorCode::Corrupted, "bad key");
    if (fnv1a32(h.key, h.key_length) != h.key_hash)
        return Error::make(ErrorCode::Corrupted, "key hash mismatch");
    h.size = kSaveStructHeaderSize + h.key_length;
    return h;
}

} // namespace nnh::detail
