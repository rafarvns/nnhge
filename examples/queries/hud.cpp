#include "hud.hpp"

#include "queries_scene.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/gfx.hpp>

namespace {

namespace gfx = nnh::gfx;
using nnh::Screen;

constexpr int kHudLayer = 8;
const nnh::Color kBand = nnh::rgba(0, 0, 0, 170);

const char* mode_title(QueriesScene::Mode mode) {
    switch (mode) {
    case QueriesScene::Mode::First: return "FIRST HIT";
    case QueriesScene::Mode::All: return "ALL HITS";
    default: return "BOX CAST";
    }
}

/// "W" when the bit is in the mask, "-" when not.
char bit(nnh::collision::Layers mask, nnh::collision::Layers b, char on) { return (mask & b) != 0 ? on : '-'; }

} // namespace

void draw_hud(const QueriesScene& scene) {
    using namespace room::bits;
    const nnh::collision::Layers m = scene.mask();
    char line[96];
    std::snprintf(line, sizeof line, "%s  mask 0x%02X [%c%c%c%c%c]  one-way %s", mode_title(scene.mode()),
                  static_cast<unsigned>(m), bit(m, kWorld, 'W'), bit(m, kWater, 'w'), bit(m, kRed, 'R'),
                  bit(m, kGreen, 'G'), bit(m, kBlue, 'B'), scene.ignore_one_way() ? "ignored" : "blocks");
    gfx::fill_rect(Screen::Top, {0, 0, 400, 12}, kBand, {.layer = kHudLayer, .ignore_camera = true});
    gfx::draw_text(Screen::Top, {4, 2}, line, {.color = nnh::colors::yellow, .layer = kHudLayer + 1,
                                               .ignore_camera = true});
    gfx::fill_rect(Screen::Top, {0, 228, 400, 12}, kBand, {.layer = kHudLayer, .ignore_camera = true});
    char first[96];
    const char* text = scene.first_line();
    const bool known = std::strncmp(text, "queries: first hit ", 19) == 0;
    std::snprintf(first, sizeof first, "first: %s", known ? text + 19 : text); // 50 characters fit the top screen
    gfx::draw_text(Screen::Top, {4, 230}, first, {.layer = kHudLayer + 1, .ignore_camera = true});

    gfx::fill_rect(Screen::Bottom, {0, 0, 320, 24}, kBand, {.layer = kHudLayer, .ignore_camera = true});
    gfx::draw_text(Screen::Bottom, {4, 2}, "Pad aim  X mode  A water  B/L/R boxes\nY one-way   touch: what is under",
                   {.layer = kHudLayer + 1, .ignore_camera = true});
    gfx::fill_rect(Screen::Bottom, {0, 206, 320, 34}, kBand, {.layer = kHudLayer, .ignore_camera = true});
    if (!scene.has_touch()) {
        gfx::draw_text(Screen::Bottom, {4, 210}, "Touch the room to ask what is there.",
                       {.layer = kHudLayer + 1, .ignore_camera = true});
        return;
    }
    gfx::draw_text(Screen::Bottom, {4, 210}, scene.under_cell(),
                   {.color = nnh::colors::yellow, .layer = kHudLayer + 1, .ignore_camera = true});
    gfx::draw_text(Screen::Bottom, {4, 222}, scene.under_detail(), {.layer = kHudLayer + 1, .ignore_camera = true});
}
