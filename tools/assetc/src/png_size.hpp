#pragma once
// The size of a PNG image from its first bytes (signature + IHDR), without decoding it. The image itself is
// checked and converted by its own asset step (nnh_check_image.cmake, tex3ds); the converter only needs its size.

#include <string_view>

namespace nnh::assetc {

/// Bytes png_size() needs.
inline constexpr int kPngHeaderBytes = 24;

/// True, with the size, when `bytes` starts with a PNG signature and an IHDR chunk of a non-zero size up to 2^31-1.
bool png_size(std::string_view bytes, int& width, int& height);

} // namespace nnh::assetc
