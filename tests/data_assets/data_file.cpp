// Test-only writer of converted data assets (data_file.hpp).

#include "data_file.hpp"

#include <cstring>

#include "core/crc32.hpp"
#include "data/data_format.hpp"

namespace data_file {

using namespace nnh::detail::datafmt;

Writer::Writer(std::uint32_t fingerprint) : bytes_(kHeaderSize, 0) {
    std::memcpy(bytes_.data(), kMagic, 4);
    put16(bytes_, kVersionAt, kVersion);
    put32(bytes_, kFingerprintAt, fingerprint);
}

Writer& Writer::u8(std::uint32_t v) {
    bytes_.push_back(static_cast<std::uint8_t>(v));
    return *this;
}

Writer& Writer::u16(std::uint32_t v) { return u8(v).u8(v >> 8); }

Writer& Writer::u32(std::uint32_t v) { return u16(v).u16(v >> 16); }

Writer& Writer::f32(float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, 4);
    return u32(bits);
}

Writer& Writer::str(const char* text) {
    const std::size_t n = std::strlen(text);
    u8(static_cast<std::uint32_t>(n));
    return raw(text, n);
}

Writer& Writer::raw(const void* data, std::size_t size) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    bytes_.insert(bytes_.end(), p, p + size);
    return *this;
}

Bytes Writer::bytes() const {
    Bytes out = bytes_;
    seal(out);
    return out;
}

void seal(Bytes& file) {
    put32(file, kSizeAt, static_cast<std::uint32_t>(file.size()));
    put32(file, kCrcAt, nnh::detail::crc32(file.data() + kHeaderSize, file.size() - kHeaderSize));
}

std::uint32_t get32(const Bytes& file, std::size_t at) {
    return file[at] | (file[at + 1] << 8) | (file[at + 2] << 16) |
           (static_cast<std::uint32_t>(file[at + 3]) << 24);
}

void put16(Bytes& file, std::size_t at, std::uint32_t v) {
    file[at] = static_cast<std::uint8_t>(v);
    file[at + 1] = static_cast<std::uint8_t>(v >> 8);
}

void put32(Bytes& file, std::size_t at, std::uint32_t v) {
    put16(file, at, v);
    put16(file, at + 2, v >> 16);
}

} // namespace data_file
