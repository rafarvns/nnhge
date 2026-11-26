// PC platform only (spec 029 FR-2, FR-13, D49): a coverage texture tinted red over blue draws exactly what an RGBA
// texture of white texels with that alpha draws, for each of the 16 coverage levels core uses (multiples of 17). Read
// back from the top screen's render target. Linked into nnh_conformance_pc (headless).

#include "context_pc.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/video.hpp"
#include "test.hpp"

#include <SDL3/SDL.h>

namespace platform = nnh::platform;

namespace {

constexpr int kSide = 16;

/// Reads the top screen's pixel (x, y) as RGBA.
bool read_pixel(SDL_Surface* s, int x, int y, Uint8 rgba[4]) {
    return s != nullptr && SDL_ReadSurfacePixel(s, x, y, &rgba[0], &rgba[1], &rgba[2], &rgba[3]);
}

platform::DrawItem2D sprite_of(platform::TextureHandle texture, float x) {
    platform::DrawItem2D item;
    item.texture = texture;
    item.region = {0, 0, kSide, kSide};
    item.x = x;
    item.y = 8;
    item.w = kSide;
    item.h = kSide;
    item.color = nnh::rgba(255, 0, 0, 255);
    return item;
}

} // namespace

NNH_TEST(pc_coverage_texture_draws_like_white_rgba_with_that_alpha) {
    nnh::AppConfig config;
    config.title = "conformance";
    config.pc_window_scale = 1;
    CHECK(platform::init(config));
    static unsigned char coverage[kSide * kSide];
    static unsigned char rgba[kSide * kSide * 4];
    for (int i = 0; i < kSide * kSide; ++i) {
        const auto a = static_cast<unsigned char>((i % 16) * 17); // every level, in every row
        coverage[i] = a;
        rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = a;
    }
    platform::TextureInfo alpha, white;
    CHECK(platform::texture_create_alpha(coverage, kSide, kSide, alpha));
    CHECK(alpha.bytes == static_cast<std::size_t>(kSide * kSide));
    CHECK(platform::texture_create_rgba(rgba, kSide, kSide, white));
    CHECK(platform::poll_events());
    platform::begin_frame(nnh::colors::blue, nnh::colors::black);
    const platform::DrawItem2D items[2] = {sprite_of(alpha.handle, 8), sprite_of(white.handle, 40)};
    platform::draw_2d(nnh::Screen::Top, items, 2);
    platform::pc::draw_on(nnh::Screen::Top);
    const SDL_Rect area{0, 0, 64, 32};
    SDL_Surface* raw = SDL_RenderReadPixels(platform::pc::context().renderer, &area);
    SDL_Surface* shot = raw != nullptr ? SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32) : nullptr;
    SDL_DestroySurface(raw);
    CHECK(shot != nullptr);
    int different = 0, levels_seen = 0;
    for (int y = 0; y < kSide; ++y) {
        for (int x = 0; x < kSide; ++x) {
            Uint8 a[4] = {}, b[4] = {};
            const bool read = read_pixel(shot, 8 + x, 8 + y, a) && read_pixel(shot, 40 + x, 8 + y, b);
            if (!read || a[0] != b[0] || a[1] != b[1] || a[2] != b[2]) ++different;
            if (y == 0 && read && (x == 0 ? a[2] == 255 : a[0] > 0)) ++levels_seen; // level 0 shows the blue
        }
    }
    CHECK(different == 0);
    CHECK(levels_seen == 16);
    SDL_DestroySurface(shot);
    platform::end_frame();
    platform::texture_destroy(alpha.handle);
    platform::texture_destroy(white.handle);
    platform::shutdown();
}
