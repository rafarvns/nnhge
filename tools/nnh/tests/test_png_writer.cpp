// PNG writer (remote/png_writer.hpp, spec 013 T3.6): the file is decoded back here — signature, chunks and
// their CRCs, IHDR, the stored zlib data (with the tests' own inflater) and every pixel.

#include <cstdint>
#include <string>
#include <vector>

#include "core/crc32.hpp"
#include "remote/png_writer.hpp"
#include "stored_inflate.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;

namespace {

struct Chunk {
    std::string type;
    std::vector<std::uint8_t> data;
    std::uint32_t crc = 0;
};

std::uint32_t be32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) << 24 | std::uint32_t(p[1]) << 16 | std::uint32_t(p[2]) << 8 | p[3];
}

/// Splits a PNG into chunks; false if the signature or a length is wrong.
bool chunks_of(const std::vector<std::uint8_t>& png, std::vector<Chunk>& out) {
    const std::uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (png.size() < 8 || !std::equal(signature, signature + 8, png.begin())) return false;
    std::size_t at = 8;
    while (at < png.size()) {
        if (at + 12 > png.size()) return false;
        const std::uint32_t length = be32(&png[at]);
        if (at + 12 + length > png.size()) return false;
        Chunk c;
        c.type.assign(png.begin() + static_cast<long>(at + 4), png.begin() + static_cast<long>(at + 8));
        c.data.assign(png.begin() + static_cast<long>(at + 8), png.begin() + static_cast<long>(at + 8 + length));
        c.crc = be32(&png[at + 8 + length]);
        out.push_back(c);
        at += 12 + length;
    }
    return true;
}

std::uint32_t crc_of(const Chunk& c) {
    std::vector<std::uint8_t> bytes(c.type.begin(), c.type.end());
    bytes.insert(bytes.end(), c.data.begin(), c.data.end());
    return nnh::detail::crc32(bytes.data(), bytes.size());
}

RgbImage gradient(std::uint16_t w, std::uint16_t h) {
    RgbImage image;
    image.width = w;
    image.height = h;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            image.rgb.insert(image.rgb.end(), {static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y),
                                               static_cast<std::uint8_t>(x ^ y)});
        }
    }
    return image;
}

} // namespace

NNH_TEST(png_has_the_signature_and_ihdr_idat_iend_with_valid_crcs) {
    std::vector<Chunk> chunks;
    CHECK(chunks_of(encode_png(gradient(3, 2)), chunks));
    CHECK(chunks.size() == 3);
    if (chunks.size() != 3) return;
    CHECK(chunks[0].type == "IHDR" && chunks[1].type == "IDAT" && chunks[2].type == "IEND");
    for (const Chunk& c : chunks) CHECK(c.crc == crc_of(c));
    CHECK(chunks[2].data.empty() && chunks[2].crc == 0xAE426082u); // the well-known IEND CRC
}

NNH_TEST(png_header_says_8_bit_rgb_without_interlace) {
    std::vector<Chunk> chunks;
    CHECK(chunks_of(encode_png(gradient(400, 240)), chunks) && !chunks.empty());
    if (chunks.empty()) return;
    const std::vector<std::uint8_t>& h = chunks[0].data;
    CHECK(h.size() == 13);
    if (h.size() != 13) return;
    CHECK(be32(&h[0]) == 400 && be32(&h[4]) == 240);
    CHECK(h[8] == 8 && h[9] == 2 && h[10] == 0 && h[11] == 0 && h[12] == 0);
}

NNH_TEST(png_data_inflates_to_filtered_rows_of_the_pixels) {
    const RgbImage image = gradient(320, 240);
    std::vector<Chunk> chunks;
    CHECK(chunks_of(encode_png(image), chunks) && chunks.size() == 3);
    if (chunks.size() != 3) return;
    std::vector<std::uint8_t> raw;
    CHECK(nnh::tool::test::inflate_stored(chunks[1].data, raw) == nnh::tool::test::Inflate::Done);
    const std::size_t row = 1 + 320 * 3;
    CHECK(raw.size() == row * 240);
    if (raw.size() != row * 240) return;
    bool same = true;
    for (std::size_t y = 0; y < 240; ++y) {
        same = same && raw[y * row] == 0; // filter: none
        same = same && std::equal(raw.begin() + static_cast<long>(y * row + 1),
                                  raw.begin() + static_cast<long>((y + 1) * row),
                                  image.rgb.begin() + static_cast<long>(y * 320 * 3));
    }
    CHECK(same);
}
