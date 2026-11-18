#include "audio/ogg_pages.hpp"

#include <cstring>

namespace nnh::detail {

namespace {

constexpr std::size_t kHeaderBytes = 27; // "OggS", version, type, granule(8), serial, sequence, crc, count

struct CrcTable {
    std::uint32_t entries[256];
    constexpr CrcTable() : entries{} {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t r = i << 24;
            for (int bit = 0; bit < 8; ++bit) r = (r & 0x80000000u) ? (r << 1) ^ 0x04C11DB7u : r << 1;
            entries[i] = r;
        }
    }
};
constexpr CrcTable kCrc;

std::uint32_t crc_update(std::uint32_t crc, const unsigned char* bytes, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) crc = (crc << 8) ^ kCrc.entries[((crc >> 24) ^ bytes[i]) & 0xFF];
    return crc;
}

std::uint32_t le32(const unsigned char* p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}

} // namespace

bool read_exact(const ByteSource& source, std::uint64_t offset, void* out, std::size_t n) {
    if (offset > source.size || n > source.size - offset) return false;
    auto* bytes = static_cast<unsigned char*>(out);
    while (n > 0) {
        const std::size_t got = source.read_at(source.context, offset, bytes, n);
        if (got == 0 || got > n) return false;
        bytes += got;
        offset += got;
        n -= got;
    }
    return true;
}

bool read_ogg_page(const ByteSource& source, std::uint64_t offset, OggPage& out) {
    unsigned char header[kHeaderBytes + 255];
    if (!read_exact(source, offset, header, kHeaderBytes)) return false;
    if (std::memcmp(header, "OggS", 4) != 0 || header[4] != 0) return false;
    const std::size_t count = header[26];
    if (!read_exact(source, offset + kHeaderBytes, header + kHeaderBytes, count)) return false;
    std::size_t body = 0;
    for (std::size_t i = 0; i < count; ++i) body += header[kHeaderBytes + i];
    const std::uint64_t size = kHeaderBytes + count + body;
    if (size > source.size - offset) return false;
    out.offset = offset;
    out.size = static_cast<std::uint32_t>(size);
    out.granule = static_cast<std::int64_t>(std::uint64_t(le32(header + 6)) | std::uint64_t(le32(header + 10)) << 32);
    return true;
}

bool ogg_page_crc_ok(const ByteSource& source, const OggPage& page, unsigned char* scratch, std::size_t scratch_size) {
    if (scratch_size < kHeaderBytes) return false;
    std::uint32_t stored = 0;
    std::uint32_t crc = 0;
    for (std::uint32_t at = 0; at < page.size;) {
        const std::size_t n = page.size - at < scratch_size ? page.size - at : scratch_size;
        if (!read_exact(source, page.offset + at, scratch, n)) return false;
        if (at == 0) { // the checksum field counts as zeros
            stored = le32(scratch + 22);
            std::memset(scratch + 22, 0, 4);
        }
        crc = crc_update(crc, scratch, n);
        at += static_cast<std::uint32_t>(n);
    }
    return crc == stored;
}

std::uint64_t ogg_last_granule(const ByteSource& source, unsigned char* scratch, std::size_t scratch_size) {
    // Search backwards, one scratch-sized window at a time, for "OggS" starting a valid page.
    std::uint64_t end = source.size;
    while (end >= 4) {
        const std::uint64_t start = end > scratch_size ? end - scratch_size : 0;
        const std::size_t n = static_cast<std::size_t>(end - start);
        if (!read_exact(source, start, scratch, n)) return 0;
        for (std::size_t i = n - 3; i-- > 0;) {
            if (std::memcmp(scratch + i, "OggS", 4) != 0) continue;
            OggPage page;
            const std::uint64_t at = start + i;
            if (read_ogg_page(source, at, page) && page.granule >= 0 && ogg_page_crc_ok(source, page, scratch, scratch_size)) {
                return static_cast<std::uint64_t>(page.granule);
            }
            // The checks reused the scratch window: read it again before searching on.
            if (!read_exact(source, start, scratch, n)) return 0;
        }
        if (start == 0) break;
        end = start + 3; // overlap so a capture pattern across windows is still found
    }
    return 0;
}

} // namespace nnh::detail
