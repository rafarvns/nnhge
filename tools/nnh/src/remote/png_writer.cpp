// RGB → PNG (see png_writer.hpp). Format: PNG specification (ISO/IEC 15948), chunks IHDR, IDAT, IEND.

#include "remote/png_writer.hpp"

#include "core/crc32.hpp"
#include "zlib_stored.hpp"

namespace nnh::tool::remote {
namespace {

void put32be(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(v >> shift));
}

/// length, type, data, CRC-32 of type + data.
void chunk(std::vector<std::uint8_t>& out, const char type[4], const std::vector<std::uint8_t>& data) {
    put32be(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t typed = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put32be(out, nnh::detail::crc32(out.data() + typed, 4 + data.size()));
}

} // namespace

std::vector<std::uint8_t> encode_png(const RgbImage& image) {
    std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<std::uint8_t> header;
    put32be(header, image.width);
    put32be(header, image.height);
    header.insert(header.end(), {8, 2, 0, 0, 0}); // 8 bits, RGB, deflate, filter method 0, no interlace
    chunk(out, "IHDR", header);

    const std::size_t row = static_cast<std::size_t>(image.width) * 3;
    std::vector<std::uint8_t> raw;
    raw.reserve((row + 1) * image.height);
    for (std::size_t y = 0; y < image.height; ++y) {
        raw.push_back(0); // filter type: none
        raw.insert(raw.end(), image.rgb.begin() + static_cast<long>(y * row),
                   image.rgb.begin() + static_cast<long>((y + 1) * row));
    }
    chunk(out, "IDAT", zlib_stored(raw.data(), raw.size()));
    chunk(out, "IEND", {});
    return out;
}

} // namespace nnh::tool::remote
