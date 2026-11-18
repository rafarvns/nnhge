#pragma once
// Ogg page headers (spec 008), read straight from a ByteSource: the file's length (last page) and the
// page to start from when seeking. Bounded: a page must lie inside the source. Used by ogg_reader.

#include <cstddef>
#include <cstdint>

#include "audio/ogg_reader.hpp"

namespace nnh::detail {

struct OggPage {
    std::uint64_t offset = 0;
    std::uint32_t size = 0;   ///< header + lacing + body
    std::int64_t granule = -1; ///< frames decoded by the end of this page; -1 = no packet ends here
};

/// Reads `n` bytes at `offset`; false on failure or if they don't all exist.
bool read_exact(const ByteSource& source, std::uint64_t offset, void* out, std::size_t n);

/// The page header at `offset`. @return false if there isn't a well-formed page that fits in the source.
bool read_ogg_page(const ByteSource& source, std::uint64_t offset, OggPage& out);

/// Whether the page's checksum matches its bytes (read through `scratch`).
bool ogg_page_crc_ok(const ByteSource& source, const OggPage& page, unsigned char* scratch, std::size_t scratch_size);

/// Granule of the last complete page with a valid checksum and a granule: the file's length in frames.
/// 0 if there is none.
std::uint64_t ogg_last_granule(const ByteSource& source, unsigned char* scratch, std::size_t scratch_size);

} // namespace nnh::detail
