#include "nnh_link/wire.hpp"

#include <cstring>

namespace nnh::link {

void Writer::u16(std::uint16_t v) {
    u8(static_cast<std::uint8_t>(v));
    u8(static_cast<std::uint8_t>(v >> 8));
}

void Writer::u32(std::uint32_t v) {
    u16(static_cast<std::uint16_t>(v));
    u16(static_cast<std::uint16_t>(v >> 16));
}

void Writer::u64(std::uint64_t v) {
    u32(static_cast<std::uint32_t>(v));
    u32(static_cast<std::uint32_t>(v >> 32));
}

void Writer::raw(const void* data, std::size_t size) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    bytes_.insert(bytes_.end(), p, p + size);
}

void Writer::str(const std::string& s) {
    const std::size_t n = s.size() < 0xFFFF ? s.size() : 0xFFFF;
    u16(static_cast<std::uint16_t>(n));
    raw(s.data(), n);
}

bool Reader::take(std::size_t n) {
    if (!ok_ || n > size_ - at_) {
        ok_ = false;
        return false;
    }
    return true;
}

std::uint8_t Reader::u8() {
    if (!take(1)) return 0;
    return data_[at_++];
}

std::uint16_t Reader::u16() {
    const std::uint16_t lo = u8();
    return static_cast<std::uint16_t>(lo | u8() << 8);
}

std::uint32_t Reader::u32() {
    const std::uint32_t lo = u16();
    return lo | std::uint32_t(u16()) << 16;
}

std::uint64_t Reader::u64() {
    const std::uint64_t lo = u32();
    return lo | std::uint64_t(u32()) << 32;
}

void Reader::raw(void* out, std::size_t size) {
    if (!take(size)) {
        std::memset(out, 0, size);
        return;
    }
    std::memcpy(out, data_ + at_, size);
    at_ += size;
}

std::string Reader::str(std::size_t max_size) {
    const std::size_t n = u16();
    if (n > max_size || !take(n)) {
        ok_ = false;
        return {};
    }
    std::string s(reinterpret_cast<const char*>(data_ + at_), n);
    at_ += n;
    return s;
}

} // namespace nnh::link
