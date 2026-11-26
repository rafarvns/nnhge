// BMFont pages (png_pages.hpp).

#include "png_pages.hpp"

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace nnh::assetc {

std::string decode_png(const std::string& bytes, int& width, int& height, std::vector<std::uint8_t>& rgba) {
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const int size = static_cast<int>(bytes.size() < (64u << 20) ? bytes.size() : (64u << 20));
    int channels = 0;
    if (!stbi_info_from_memory(data, size, &width, &height, &channels)) return "not a PNG image";
    if (width < 1 || height < 1 || width > 4096 || height > 4096) return "a page larger than 4096x4096";
    stbi_uc* pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (pixels == nullptr) return std::string("the PNG can't be decoded (") + stbi_failure_reason() + ")";
    rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    stbi_image_free(pixels);
    return "";
}

} // namespace nnh::assetc
