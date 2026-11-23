#include "hud.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>

namespace {

constexpr nnh::Screen kBottom = nnh::Screen::Bottom;

} // namespace

void Hud::attach() {
    coin_sub_ = nnh::events::subscribe<&Hud::on_coin>(this);
    hit_sub_ = nnh::events::subscribe<&Hud::on_hit>(this);
    restart_sub_ = nnh::events::subscribe<&Hud::on_restart>(this);
}

void Hud::detach() {
    coin_sub_.reset();
    hit_sub_.reset();
    restart_sub_.reset();
}

void Hud::on_coin(const CoinCollected& e) {
    ++coins_;
    score_ += e.value;
}

void Hud::on_hit(const PlayerHit& e) {
    hits_ = e.hits;
}

void Hud::on_restart(const LevelRestarted& e) {
    coins_ = 0;
    hits_ = 0;
    total_ = e.coins;
    score_ = 0;
}

void Hud::draw(const nnh::Tilemap& map, const char* about, int entities) const {
    namespace gfx = nnh::gfx;
    const nnh::Properties props = map.properties(); // the map's own (FR-6)
    gfx::draw_text(kBottom, {12, 10}, props.get_string("title", "A LEVEL"),
                   {.color = nnh::rgb(250, 220, 120), .scale = 2, .shadow = true});
    gfx::draw_text(kBottom, {12, 32}, about);
    char text[48];
    std::snprintf(text, sizeof text, "coins %d / %d", coins_, total_);
    gfx::draw_text(kBottom, {12, 50}, text, {.scale = 2});
    std::snprintf(text, sizeof text, hits_ > 0 ? "score %d   hits %d" : "score %d", score_, hits_);
    gfx::draw_text(kBottom, {12, 72}, text);
    std::snprintf(text, sizeof text, "entities %d", entities);
    gfx::draw_text(kBottom, {308, 72}, text, {.color = nnh::rgb(150, 220, 160), .align = nnh::TextAlign::Right});
    if (props.has("music")) {
        std::snprintf(text, sizeof text, "music: \"%s\" (a map property)", props.get_string("music"));
        gfx::draw_text(kBottom, {12, 84}, text, {.color = nnh::rgb(150, 160, 180)});
    }
}

void Hud::draw_controls() const {
    nnh::gfx::draw_text(kBottom, {12, 150},
                        "D-Pad / Circle Pad  run\nA or B    jump\nDown + A  drop through planks\n"
                        "X         replay (level 1 + bonus)\nSELECT    next demo   START  exit",
                        {.color = nnh::rgb(190, 200, 220)});
}
