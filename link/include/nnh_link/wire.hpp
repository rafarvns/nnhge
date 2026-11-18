#pragma once
// Bytes on the wire for NNH Link (spec 011): little-endian integers and length-prefixed strings, written
// to a buffer and read back with bounds checks — a Reader never reads past its input; any problem makes
// ok() false. Pure; unit tested in link/tests/test_messages.cpp.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nnh::link {

class Writer {
public:
    void u8(std::uint8_t v) { bytes_.push_back(v); }
    void u16(std::uint16_t v);
    void u32(std::uint32_t v);
    void u64(std::uint64_t v);
    void raw(const void* data, std::size_t size);
    /// u16 length + bytes (callers keep strings short; see the limits in messages.hpp).
    void str(const std::string& s);
    std::vector<std::uint8_t>& bytes() { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
};

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
    explicit Reader(const std::vector<std::uint8_t>& v) : data_(v.data()), size_(v.size()) {}

    std::uint8_t u8();
    std::uint16_t u16();
    std::uint32_t u32();
    std::uint64_t u64();
    void raw(void* out, std::size_t size);
    /// A string of at most `max_size` bytes (longer: not ok).
    std::string str(std::size_t max_size);

    bool ok() const { return ok_; }
    /// Everything read and nothing left over.
    bool done() const { return ok_ && at_ == size_; }
    std::size_t left() const { return size_ - at_; }

private:
    bool take(std::size_t n);
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t at_ = 0;
    bool ok_ = true;
};

} // namespace nnh::link
