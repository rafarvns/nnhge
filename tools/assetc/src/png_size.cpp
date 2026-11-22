// The size of a PNG image from its header (see png_size.hpp).

#include "png_size.hpp"

#include <cstdint>

namespace nnh::assetc {

namespace {

std::uint32_t big_endian(std::string_view bytes, std::size_t at) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) value = value << 8 | static_cast<unsigned char>(bytes[at + i]);
    return value;
}

} // namespace

bool png_size(std::string_view bytes, int& width, int& height) {
    static constexpr char kSignature[] = "\x89PNG\r\n\x1a\n";
    if (bytes.size() < static_cast<std::size_t>(kPngHeaderBytes)) return false;
    if (bytes.substr(0, 8) != std::string_view(kSignature, 8)) return false;
    if (big_endian(bytes, 8) != 13 || bytes.substr(12, 4) != "IHDR") return false;
    const std::uint32_t w = big_endian(bytes, 16), h = big_endian(bytes, 20);
    if (w == 0 || h == 0 || w > 0x7FFFFFFFu || h > 0x7FFFFFFFu) return false;
    width = static_cast<int>(w);
    height = static_cast<int>(h);
    return true;
}

} // namespace nnh::assetc
