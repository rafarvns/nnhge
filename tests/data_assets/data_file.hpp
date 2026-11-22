#pragma once
// Test-only writer of converted data assets (`NNHD`, engine/core/src/data/data_format.hpp), byte by byte and
// field index by field index, so the tests can build good and damaged files without the converter. seal()
// fixes the header's size and CRC (after a test changed the body). The real writer is nnh-assetc's (stage 5).
// Also writes data indexes `NNHX` (index_bytes), whose real writer is nnh-assetc's data-index step (stage 6).

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace data_file {

using Bytes = std::vector<std::uint8_t>;

class Writer {
public:
    /// Starts a file whose header carries `fingerprint` (version 1, flags 0; size and CRC set by bytes()).
    explicit Writer(std::uint32_t fingerprint);

    Writer& u8(std::uint32_t v);
    Writer& u16(std::uint32_t v);
    Writer& u32(std::uint32_t v);
    Writer& f32(float v);
    /// A string value: u8 length + bytes (no terminator).
    Writer& str(const char* text);
    Writer& raw(const void* data, std::size_t size);
    /// A struct value's count, then each field's index (followed by its value).
    Writer& fields(int count) { return u8(static_cast<std::uint32_t>(count)); }
    Writer& field(int index) { return u8(static_cast<std::uint32_t>(index)); }

    /// The finished file (size and CRC fixed).
    Bytes bytes() const;

private:
    Bytes bytes_;
};

/// Rewrites the header's size and CRC to match `file`'s length and body (at least a header's worth of bytes).
void seal(Bytes& file);

/// Reads/writes a header field (little-endian u16 or u32 at `at`).
std::uint32_t get32(const Bytes& file, std::size_t at);
void put16(Bytes& file, std::size_t at, std::uint32_t v);
void put32(Bytes& file, std::size_t at, std::uint32_t v);

// ---- The data index `.nnh_data` (engine/core/src/data/index_format.hpp) ------------------------------------

struct IndexType {
    std::string key;
    std::uint32_t fingerprint = 0;
    std::vector<std::string> paths; ///< written in this order (keep them sorted for a valid index)
};

struct IndexSpec {
    std::vector<IndexType> types;
    std::vector<std::string> media[3]; ///< textures, sounds, music (in this order, sorted for a valid index)
    std::uint32_t largest = 0;
};

/// An index of `spec`: hashes computed with fnv1a32, the lookup sorted, size and CRC set.
Bytes index_bytes(const IndexSpec& spec);

/// Rewrites an index's size and CRC to match `file` (at least a header's worth of bytes).
void seal_index(Bytes& file);

/// Where the parts of an index of `spec` start (to damage them in tests).
std::size_t index_types_at();
std::size_t index_entries_at(const IndexSpec& spec);
std::size_t index_lookup_at(const IndexSpec& spec);
std::size_t index_strings_at(const IndexSpec& spec);

} // namespace data_file
