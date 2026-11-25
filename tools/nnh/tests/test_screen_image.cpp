// Screenshot pixels → RGB (remote/screen_image.hpp, spec 013 T3.6): both layouts give the same image for a
// known pattern; wrong screens, layouts and sizes are refused.

#include <cstdint>
#include <string>
#include <vector>

#include "remote/screen_image.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;
namespace wire = nnh::detail::remote;

namespace {

/// The test pattern: a different colour for every pixel and screen.
void pattern(int screen, int x, int y, std::uint8_t rgb[3]) {
    rgb[0] = static_cast<std::uint8_t>(x * 7);
    rgb[1] = static_cast<std::uint8_t>(y * 3 + x / 256);
    rgb[2] = static_cast<std::uint8_t>(screen == 0 ? 0x40 : 0x80);
}

wire::ScreenHeader header(std::uint8_t screen, std::uint8_t layout) {
    wire::ScreenHeader h;
    h.screen = screen;
    h.layout = layout;
    h.width = screen == 0 ? 400 : 320;
    h.height = 240;
    return h;
}

/// What the PC sends: RGBA rows, top-down.
std::vector<std::uint8_t> rgba_rows(int screen, int w, int h) {
    std::vector<std::uint8_t> out;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            std::uint8_t c[3];
            pattern(screen, x, y, c);
            out.insert(out.end(), {c[0], c[1], c[2], 0xFF});
        }
    }
    return out;
}

/// What the 3DS sends: BGR columns from the left, each from the bottom.
std::vector<std::uint8_t> bgr_columns(int screen, int w, int h) {
    std::vector<std::uint8_t> out;
    for (int x = 0; x < w; ++x) {
        for (int y = h - 1; y >= 0; --y) {
            std::uint8_t c[3];
            pattern(screen, x, y, c);
            out.insert(out.end(), {c[2], c[1], c[0]});
        }
    }
    return out;
}

bool is_pattern(const RgbImage& image, int screen) {
    if (image.rgb.size() != static_cast<std::size_t>(image.width) * image.height * 3) return false;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            std::uint8_t c[3];
            pattern(screen, x, y, c);
            const std::uint8_t* p = &image.rgb[(static_cast<std::size_t>(y) * image.width + x) * 3];
            if (p[0] != c[0] || p[1] != c[1] || p[2] != c[2]) return false;
        }
    }
    return true;
}

} // namespace

NNH_TEST(screen_image_reads_rgba_rows) {
    for (int screen : {0, 1}) {
        RgbImage image;
        const int w = screen == 0 ? 400 : 320;
        CHECK(screen_to_rgb(header(screen, 1), rgba_rows(screen, w, 240), image).empty());
        CHECK(image.width == w && image.height == 240 && is_pattern(image, screen));
    }
}

NNH_TEST(screen_image_reads_bgr_columns_from_the_bottom) {
    for (int screen : {0, 1}) {
        RgbImage image;
        const int w = screen == 0 ? 400 : 320;
        CHECK(screen_to_rgb(header(screen, 2), bgr_columns(screen, w, 240), image).empty());
        CHECK(image.width == w && image.height == 240 && is_pattern(image, screen));
    }
}

NNH_TEST(screen_image_refuses_wrong_sizes) {
    RgbImage image;
    std::vector<std::uint8_t> pixels = rgba_rows(0, 400, 240);
    pixels.pop_back();
    CHECK(screen_to_rgb(header(0, 1), pixels, image) == "the top screen should be 384000 bytes, not 383999");
    CHECK(!screen_to_rgb(header(1, 1), rgba_rows(0, 400, 240), image).empty());   // too many bytes
    CHECK(!screen_to_rgb(header(0, 2), rgba_rows(0, 400, 240), image).empty());   // RGBA size as BGR
    wire::ScreenHeader small = header(0, 1);
    small.width = 2;
    small.height = 2;
    CHECK(screen_to_rgb(small, std::vector<std::uint8_t>(16), image) == "the top screen is 400x240, not 2x2");
    wire::ScreenHeader short_one = header(0, 1);
    short_one.height = 239;
    CHECK(!screen_to_rgb(short_one, rgba_rows(0, 400, 240), image).empty());
    wire::ScreenHeader swapped = header(1, 1);
    swapped.width = 400;
    CHECK(!screen_to_rgb(swapped, rgba_rows(0, 400, 240), image).empty());
}

NNH_TEST(screen_image_reads_the_right_eye_as_a_top_screen) {
    RgbImage image;
    wire::ScreenHeader h = header(2, 2); // screen 2: the top screen's right eye (spec 026)
    h.width = 400;
    CHECK(screen_to_rgb(h, bgr_columns(0, 400, 240), image).empty());
    CHECK(image.width == 400 && is_pattern(image, 0));
    h.width = 320;
    CHECK(screen_to_rgb(h, bgr_columns(0, 320, 240), image) == "the top (right eye) screen is 400x240, not 320x240");
}

NNH_TEST(screen_image_refuses_unknown_screens_and_layouts) {
    RgbImage image;
    CHECK(screen_to_rgb(header(3, 1), {}, image) == "unknown screen 3");
    CHECK(screen_to_rgb(header(0, 3), rgba_rows(0, 400, 240), image) == "unknown pixel layout 3");
    CHECK(screen_to_rgb(header(0, 0), rgba_rows(0, 400, 240), image) == "unknown pixel layout 0");
}
