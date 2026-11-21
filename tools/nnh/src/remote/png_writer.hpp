#pragma once
// RGB images → PNG files for console screenshots (spec 013 FR-2), without an image library: 8-bit RGB,
// filter 0 on every row, the image data in stored (uncompressed) zlib blocks (zlib_stored.hpp, D23), chunk
// CRCs by core's crc32. Pure; tested in tools/nnh/tests/test_png_writer.cpp (decoded back).

#include <cstdint>
#include <vector>

#include "remote/screen_image.hpp"

namespace nnh::tool::remote {

/// The whole PNG file. A 400×240 screen is about 288 KB.
std::vector<std::uint8_t> encode_png(const RgbImage& image);

} // namespace nnh::tool::remote
