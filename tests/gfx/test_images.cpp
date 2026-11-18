// The images "embedded" in the test program, in the fake platform's format: "FAKE", width, height
// (16-bit little endian). Games get this table from nnh_add_game(... IMAGES ...).

#include "gfx/image_registry.hpp"

namespace {

constexpr unsigned char kHero[] = {'F', 'A', 'K', 'E', 64, 0, 32, 0};
constexpr unsigned char kTile[] = {'F', 'A', 'K', 'E', 16, 0, 16, 0};
constexpr unsigned char kTooSmall[] = {'F', 'A', 'K', 'E', 4, 0, 4, 0};
constexpr unsigned char kNotPowerOfTwo[] = {'F', 'A', 'K', 'E', 100, 0, 64, 0};
constexpr unsigned char kGarbage[] = {'P', 'N', 'G', '?'};

constexpr nnh::detail::EmbeddedImage kImages[] = {
    {"hero", kHero, sizeof kHero},
    {"tile", kTile, sizeof kTile},
    {"too_small", kTooSmall, sizeof kTooSmall},
    {"npot", kNotPowerOfTwo, sizeof kNotPowerOfTwo},
    {"garbage", kGarbage, sizeof kGarbage},
};

} // namespace

const nnh::detail::EmbeddedImage* nnh::detail::embedded_images(int& count) {
    count = static_cast<int>(sizeof kImages / sizeof kImages[0]);
    return kImages;
}
