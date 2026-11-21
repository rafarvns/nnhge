#pragma once
// Screenshots from the console remote (spec 013 FR-2): the pixels of a Screen record, in the layout the
// platform read them back in, → RGB rows, top-down. Pure; tested in tools/nnh/tests/test_screen_image.cpp.
//
//   layout 1 (PC): RGBA8 rows, top-down, bytes R G B A.
//   layout 2 (3DS): the framebuffer's order — BGR8 columns from left to right, each from bottom to top.

#include <cstdint>
#include <string>
#include <vector>

#include "remote/remote_wire.hpp"

namespace nnh::tool::remote {

struct RgbImage {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> rgb; ///< width × height × 3, rows top-down
};

inline constexpr std::uint8_t kLayoutRgba8Rows = 1;
inline constexpr std::uint8_t kLayoutBgr8ColumnsBottomUp = 2;

/// Converts a Screen record's pixels. Only the console's two screens are accepted (screen 0 = top 400×240,
/// 1 = bottom 320×240), in a known layout, with exactly width × height × bytes-per-pixel bytes.
/// @return "" and fills `out`, or why the record isn't a screenshot.
std::string screen_to_rgb(const nnh::detail::remote::ScreenHeader& header, const std::vector<std::uint8_t>& pixels,
                          RgbImage& out);

} // namespace nnh::tool::remote
