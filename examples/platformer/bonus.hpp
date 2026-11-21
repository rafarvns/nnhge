#pragma once
// Playing a level: the player, the camera following it (clamped to the map), coins collected by overlap,
// the HUD on the bottom screen. Map layers draw around the player: clouds and hills behind, ground, then
// coins and the player, then the front layer over them.

#include "art.hpp"
#include "player.hpp"

#include <nnh/tilemap.hpp>

class Bonus {
  public:
    static constexpr int kMaxCoins = 64;

    Bonus(const Art& art, const nnh::Tilemap& map);
    void update();
    void draw() const;

  private:
    void restart();

    const Art& art_;
    const nnh::Tilemap& map_;
    nnh::collision::Grid grid_{};
    Player player_;
    nnh::Vec2 spawn_{};
    nnh::Rect coins_[kMaxCoins] = {};
    bool taken_[kMaxCoins] = {};
    int coin_count_ = 0;
    int collected_ = 0;
    int layers_[4] = {};
};
