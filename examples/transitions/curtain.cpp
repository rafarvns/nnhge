#include "curtain.hpp"

#include <nnh/gfx.hpp>

namespace tr = nnh::transition;

namespace {

constexpr int kTile = 16;
// Tile indexes of art/tiles.png (4 per row).
constexpr int kCloth = 6, kHem = 7, kBarLeft = 8, kBarMid = 9, kBarRight = 10;

nnh::Region tile(int index) { return {(index % 4) * kTile, (index / 4) * kTile, kTile, kTile}; }

} // namespace

tr::CustomEffect Curtain::effect() {
    return [this](const tr::CustomFrame& f) { draw(f); };
}

void Curtain::draw(const tr::CustomFrame& f) const {
    if (tiles_ == nullptr) return;
    const int width = nnh::screen_width(f.screen);
    // The curtain's bottom edge: 0 at coverage 0, the whole screen (and the hem below it) at 1.
    const int drop = static_cast<int>(f.coverage * static_cast<float>(240 + kTile));
    for (int y = drop - kTile; y > -kTile; y -= kTile) {
        const int index = y == drop - kTile ? kHem : kCloth;
        for (int x = 0; x < width; x += kTile) {
            nnh::gfx::draw(f.screen, *tiles_, {static_cast<float>(x), static_cast<float>(y)},
                           {.region = tile(index), .layer = 15, .ignore_camera = true});
        }
    }
    if (f.phase == tr::Phase::Covered && f.loading < 1.0f) draw_bar(f.screen, f.loading, 15);
}

void Curtain::draw_bar(nnh::Screen screen, float progress, int layer) const {
    if (tiles_ == nullptr) return;
    const float width = static_cast<float>(nnh::screen_width(screen));
    const float x0 = width / 2 - 80;
    for (int i = 0; i < 10; ++i) {
        const int index = i == 0 ? kBarLeft : (i == 9 ? kBarRight : kBarMid);
        nnh::gfx::draw(screen, *tiles_, {x0 + static_cast<float>(i * kTile), 200},
                       {.region = tile(index), .layer = layer, .ignore_camera = true});
    }
    const float fill = 154.0f * (progress < 0 ? 0 : (progress > 1 ? 1 : progress));
    nnh::gfx::fill_rect(screen, {x0 + 3, 204, fill, 8}, nnh::rgb(250, 200, 60),
                        {.layer = layer, .ignore_camera = true});
}
