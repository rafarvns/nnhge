// Screenshot pixels → RGB (see screen_image.hpp).

#include "remote/screen_image.hpp"

namespace nnh::tool::remote {

std::string screen_to_rgb(const nnh::detail::remote::ScreenHeader& h, const std::vector<std::uint8_t>& pixels,
                          RgbImage& out) {
    if (h.screen > 2) return "unknown screen " + std::to_string(h.screen);
    static constexpr const char* kNames[3] = {"top", "bottom", "top (right eye)"}; // 2: spec 026
    const char* name = kNames[h.screen];
    const int w = h.screen == 1 ? 320 : 400;
    const int ht = 240;
    if (h.width != w || h.height != ht) {
        return std::string("the ") + name + " screen is " + std::to_string(w) + "x" + std::to_string(ht) +
               ", not " + std::to_string(h.width) + "x" + std::to_string(h.height);
    }
    if (h.layout != kLayoutRgba8Rows && h.layout != kLayoutBgr8ColumnsBottomUp) {
        return "unknown pixel layout " + std::to_string(h.layout);
    }
    const std::size_t bpp = h.layout == kLayoutRgba8Rows ? 4 : 3;
    const std::size_t want = static_cast<std::size_t>(w) * ht * bpp;
    if (pixels.size() != want) {
        return std::string("the ") + name + " screen should be " + std::to_string(want) + " bytes, not " +
               std::to_string(pixels.size());
    }
    out.width = static_cast<std::uint16_t>(w);
    out.height = static_cast<std::uint16_t>(ht);
    out.rgb.resize(static_cast<std::size_t>(w) * ht * 3);
    for (int y = 0; y < ht; ++y) {
        for (int x = 0; x < w; ++x) {
            std::uint8_t* d = &out.rgb[(static_cast<std::size_t>(y) * w + x) * 3];
            if (h.layout == kLayoutRgba8Rows) {
                const std::uint8_t* s = &pixels[(static_cast<std::size_t>(y) * w + x) * 4];
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
            } else {
                const std::uint8_t* s = &pixels[(static_cast<std::size_t>(x) * ht + (ht - 1 - y)) * 3];
                d[0] = s[2];
                d[1] = s[1];
                d[2] = s[0];
            }
        }
    }
    return "";
}

} // namespace nnh::tool::remote
