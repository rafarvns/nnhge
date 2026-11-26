#pragma once
// BMFont pages (spec 029 FR-1; plan question 2): a PNG decoded to RGBA8 by the vendored stb_image (PNG only, no stdio),
// its size checked before decoding (at most 4096 × 4096). Tests: tests/test_bmfont_convert.cpp.

#include <cstdint>
#include <string>
#include <vector>

namespace nnh::assetc {

/// @return "" (and the pixels) or the problem.
std::string decode_png(const std::string& bytes, int& width, int& height, std::vector<std::uint8_t>& rgba);

} // namespace nnh::assetc
