#include "bonus.hpp"

#include "bonus_level.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL(bonus);

namespace {

constexpr nnh::Screen kTop = nnh::Screen::Top;
constexpr nnh::Screen kBottom = nnh::Screen::Bottom;

// Draw layers, back to front.
enum DrawLayer : int { kBackdrop = 0, kGround = 2, kCoins = 3, kPlayer = 4, kFront = 5 };

} // namespace

Bonus::Bonus(const Art& art, const nnh::Tilemap& map) : art_(art), map_(map) {
    grid_ = map.collision_grid();
    grid_.outside = nnh::collision::Cell::Solid; // walls the level in (and a floor under the pits)
    for (int i = 0; i < 4; ++i)
        layers_[i] = map.find_tile_layer(kBonusLayers[i]);
    const int spawn = map.find_object("spawn");
    if (spawn >= 0) spawn_ = {map.object(spawn).bounds.x, map.object(spawn).bounds.y};
    int found[kMaxCoins];
    coin_count_ = map.find_objects("coin", found, kMaxCoins);
    if (coin_count_ > kMaxCoins) coin_count_ = kMaxCoins;
    for (int i = 0; i < coin_count_; ++i)
        coins_[i] = map.object(found[i]).bounds;
    restart();
}

void Bonus::restart() {
    player_.spawn(spawn_);
    for (bool& taken : taken_)
        taken = false;
    collected_ = 0;
}

void Bonus::update() {
    player_.update(grid_);
    const nnh::Rect box = player_.box();
    if (box.y + box.h >= static_cast<float>(map_.height() * map_.tile_height())) {
        NNH_LOG_INFO(bonus, "fell into the water: back to the start");
        restart();
        return;
    }
    for (int i = 0; i < coin_count_; ++i) {
        if (!taken_[i] && nnh::collision::overlaps(box, coins_[i])) {
            taken_[i] = true;
            ++collected_;
        }
    }
    // The camera follows the player, clamped to the map.
    const float map_w = static_cast<float>(map_.width() * map_.tile_width());
    float x = box.x + box.w / 2 - nnh::screen_width(kTop) / 2.0f;
    if (x > map_w - nnh::screen_width(kTop)) x = map_w - nnh::screen_width(kTop);
    if (x < 0) x = 0;
    nnh::gfx::set_camera(kTop, {{x, 0}});
}

void Bonus::draw() const {
    namespace gfx = nnh::gfx;
    gfx::draw_tilemap(kTop, map_, {.map_layer = layers_[0], .layer = kBackdrop});
    gfx::draw_tilemap(kTop, map_, {.map_layer = layers_[1], .layer = kBackdrop});
    gfx::draw_tilemap(kTop, map_, {.map_layer = layers_[2], .layer = kGround});
    for (int i = 0; i < coin_count_; ++i) {
        if (!taken_[i])
            gfx::draw(kTop, art_.tiles, {coins_[i].x - 2, coins_[i].y - 2},
                      {.region = tile::region(tile::Coin), .layer = kCoins});
    }
    player_.draw(kTop, art_.player, kPlayer);
    gfx::draw_tilemap(kTop, map_, {.map_layer = layers_[3], .layer = kFront});

    const nnh::TextOptions title{.color = nnh::rgb(250, 220, 120), .scale = 2, .shadow = true};
    gfx::draw_text(kBottom, {12, 10}, "BONUS LEVEL", title);
    gfx::draw_text(kBottom, {12, 30}, "built in code: no file, no editor");
    char coins[48];
    std::snprintf(coins, sizeof coins, "coins %d / %d", collected_, coin_count_);
    gfx::draw_text(kBottom, {12, 56}, coins, {.scale = 2});
    gfx::draw_text(kBottom, {12, 150},
                   "D-Pad / Circle Pad  run\nA or B    jump\nDown + A  drop through planks\n\n"
                   "SELECT    next demo (bench, flips)\nSTART     exit",
                   {.color = nnh::rgb(190, 200, 220)});
}
