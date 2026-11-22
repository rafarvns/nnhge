// PC platform only: draw_tiles puts every pixel where Tiled would (spec 012 FR-2, FR-3). An 8x8 texture with
// a different color per pixel is drawn in the 8 orientations (and at a clipped position) on the top screen's
// render target, then read back. Linked into nnh_conformance_pc (headless: offscreen video, software
// renderer).

#include "context_pc.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/video.hpp"
#include "test.hpp"

#include <SDL3/SDL.h>

#include <initializer_list>

namespace platform = nnh::platform;

namespace {

constexpr int kSize = 8;
constexpr int kStride = 12; // quads are 12 px apart

/// The texture's pixel (x, y): red = x, green = y, so every pixel differs.
void pixel_color(int x, int y, Uint8& r, Uint8& g, Uint8& b) {
    r = static_cast<Uint8>(16 + x * 32);
    g = static_cast<Uint8>(16 + y * 32);
    b = 200;
}

/// Tiled's rule, inverted: the source pixel shown at (dx, dy) of a tile with orientation `o`.
void source_of(int o, int dx, int dy, int& sx, int& sy) {
    const int u = (o & platform::kTileOrientFlipX) ? kSize - 1 - dx : dx; // the mirrors came last
    const int v = (o & platform::kTileOrientFlipY) ? kSize - 1 - dy : dy;
    const bool diagonal = (o & platform::kTileOrientDiagonal) != 0; // the transpose came first
    sx = diagonal ? v : u;
    sy = diagonal ? u : v;
}

struct Readback {
    SDL_Surface* surface = nullptr;
    explicit Readback(const SDL_Rect& area) {
        SDL_Renderer* renderer = platform::pc::context().renderer;
        platform::pc::draw_on(nnh::Screen::Top);
        SDL_Surface* raw = SDL_RenderReadPixels(renderer, &area);
        if (raw != nullptr) surface = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(raw);
    }
    ~Readback() { SDL_DestroySurface(surface); }
    bool at(int x, int y, Uint8& r, Uint8& g, Uint8& b) const {
        Uint8 a = 0;
        return surface != nullptr && SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a);
    }
};

int g_mismatches = 0;

void expect_tile(const Readback& shot, int left, int top, int orient) {
    for (int dy = 0; dy < kSize; ++dy) {
        for (int dx = 0; dx < kSize; ++dx) {
            int sx = 0, sy = 0;
            source_of(orient, dx, dy, sx, sy);
            Uint8 er = 0, eg = 0, eb = 0, r = 0, g = 0, b = 0;
            pixel_color(sx, sy, er, eg, eb);
            const bool same = shot.at(left + dx, top + dy, r, g, b) && r == er && g == eg && b == eb;
            if (!same && ++g_mismatches <= 4) {
                SDL_Log("orient %d pixel (%d,%d): got %d,%d,%d want %d,%d,%d", orient, dx, dy, r, g, b, er,
                        eg, eb);
            }
        }
    }
}

} // namespace

NNH_TEST(pc_draw_tiles_places_every_pixel_of_the_eight_orientations_like_tiled) {
    nnh::AppConfig config;
    config.title = "conformance";
    config.pc_window_scale = 1;
    CHECK(platform::init(config));
    static unsigned char pixels[kSize * kSize * 4];
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            unsigned char* p = pixels + (y * kSize + x) * 4;
            pixel_color(x, y, p[0], p[1], p[2]);
            p[3] = 255;
        }
    }
    platform::TextureInfo info;
    CHECK(platform::texture_create_rgba(pixels, kSize, kSize, info));
    CHECK(platform::poll_events());
    platform::begin_frame(nnh::colors::black, nnh::colors::black);
    platform::TileQuad2D quads[9];
    for (int o = 0; o < 8; ++o)
        quads[o] = {static_cast<std::int16_t>(o * kStride), 4, 0, 0, static_cast<std::uint8_t>(o)};
    quads[8] = {-3, 20, 0, 0, platform::kTileOrientDiagonal}; // its left 3 columns are off screen
    platform::TileBatch2D batch;
    batch.texture = info.handle;
    batch.tile_w = kSize;
    batch.tile_h = kSize;
    batch.dst_w = kSize;
    batch.dst_h = kSize;
    batch.color = nnh::colors::white;
    batch.quads = quads;
    batch.count = 9;
    platform::draw_tiles(nnh::Screen::Top, batch);
    {
        const Readback shot(SDL_Rect{0, 0, 8 * kStride, 32});
        CHECK(shot.surface != nullptr);
        g_mismatches = 0;
        for (int o = 0; o < 8; ++o)
            expect_tile(shot, o * kStride, 4, o);
        CHECK(g_mismatches == 0);
        // The clipped tile: screen column c shows the tile's column c + 3.
        Uint8 r = 0, g = 0, b = 0, er = 0, eg = 0, eb = 0;
        int sx = 0, sy = 0;
        source_of(platform::kTileOrientDiagonal, 3, 0, sx, sy);
        pixel_color(sx, sy, er, eg, eb);
        CHECK(shot.at(0, 20, r, g, b) && r == er && g == eg && b == eb);
        CHECK(shot.at(kSize - 3, 20, r, g, b) && r == 0 && g == 0 && b == 0); // nothing past the tile
    }
    platform::end_frame();
    platform::texture_destroy(info.handle);
    platform::shutdown();
}

namespace {

bool lit(const Readback& shot, int x, int y) {
    Uint8 r = 0, g = 0, b = 0;
    return shot.at(x, y, r, g, b) && b == 200; // every texel has blue 200; the clear color is black
}

/// Counts pixels of [x0, x1) × [y0, y1) that are lit (`want` true) or not.
int count_lit(const Readback& shot, int x0, int y0, int x1, int y1, bool want) {
    int n = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (lit(shot, x, y) == want) ++n;
    return n;
}

} // namespace

// Zoom (spec 018, decision D33): quads are drawn at the batch's destination size, plus one pixel per widen/taller bit.
NNH_TEST(pc_draw_tiles_draws_zoomed_quads_exactly_on_their_rectangles) {
    nnh::AppConfig config;
    config.title = "conformance";
    config.pc_window_scale = 1;
    CHECK(platform::init(config));
    static unsigned char pixels[kSize * kSize * 4];
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            unsigned char* p = pixels + (y * kSize + x) * 4;
            pixel_color(x, y, p[0], p[1], p[2]);
            p[3] = 255;
        }
    }
    platform::TextureInfo info;
    CHECK(platform::texture_create_rgba(pixels, kSize, kSize, info));
    CHECK(platform::poll_events());
    platform::begin_frame(nnh::colors::black, nnh::colors::black);
    platform::TileBatch2D batch;
    batch.texture = info.handle;
    batch.tile_w = kSize;
    batch.tile_h = kSize;
    batch.color = nnh::colors::white;
    // Zoom 2: one 16 × 16 quad at (0, 0).
    platform::TileQuad2D doubled{0, 0, 0, 0, 0};
    batch.dst_w = batch.dst_h = 2 * kSize;
    batch.quads = &doubled;
    batch.count = 1;
    platform::draw_tiles(nnh::Screen::Top, batch);
    // Zoom 1.5 (dst 12): a widened quad (13 × 12) at (40, 0) and a diagonal widened one at (40, 20).
    platform::TileQuad2D wide[2] = {{40, 0, 0, 0, platform::kTileOrientWiden},
                                    {40, 20, 0, 0, platform::kTileOrientWiden | platform::kTileOrientDiagonal}};
    batch.dst_w = batch.dst_h = 12;
    batch.quads = wide;
    batch.count = 2;
    platform::draw_tiles(nnh::Screen::Top, batch);
    {
        const Readback shot(SDL_Rect{0, 0, 64, 40});
        CHECK(shot.surface != nullptr);
        // Zoom 2: texel (tx, ty) fills the 2 × 2 square at (2tx, 2ty).
        int wrong = 0;
        for (int y = 0; y < 2 * kSize; ++y) {
            for (int x = 0; x < 2 * kSize; ++x) {
                Uint8 r = 0, g = 0, b = 0, er = 0, eg = 0, eb = 0;
                pixel_color(x / 2, y / 2, er, eg, eb);
                if (!(shot.at(x, y, r, g, b) && r == er && g == eg && b == eb)) ++wrong;
            }
        }
        CHECK(wrong == 0);
        CHECK(count_lit(shot, 16, 0, 18, 18, true) == 0 && count_lit(shot, 0, 16, 18, 18, true) == 0);
        // The widened quads cover exactly 13 × 12 pixels, nothing around them.
        for (int top : {0, 20}) {
            CHECK(count_lit(shot, 40, top, 53, top + 12, false) == 0);
            CHECK(count_lit(shot, 39, top, 40, top + 12, true) == 0 && count_lit(shot, 53, top, 54, top + 12, true) == 0);
            CHECK(count_lit(shot, 40, top + 12, 53, top + 13, true) == 0);
        }
    }
    platform::end_frame();
    platform::texture_destroy(info.handle);
    platform::shutdown();
}
