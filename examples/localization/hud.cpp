#include "hud.hpp"

#include <nnh/gfx.hpp>
#include <nnh/localization.hpp>

void Hud::draw(int frame) const {
    const nnh::FontRef hud = nnh::loc::font("hud");
    char score[32], coins[32];
    nnh::loc::format(score, sizeof score, "hud.score", nnh::loc::arg("score", 1234500 + frame));
    nnh::loc::format(coins, sizeof coins, "hud.coins", nnh::loc::arg("coins", 12000 + frame / 10));
    const nnh::TextOptions o{.color = nnh::rgb(255, 230, 140), .scale = 2, .shadow = true, .layer = 14, .font = hud};
    nnh::gfx::draw_text(nnh::Screen::Top, {6, 4}, score, o);
    nnh::TextOptions right = o;
    right.align = nnh::TextAlign::Right;
    nnh::gfx::draw_text(nnh::Screen::Top, {394, 4}, coins, right);
}
