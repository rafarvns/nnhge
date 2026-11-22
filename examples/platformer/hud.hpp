#pragma once
// The levels' HUD on the bottom screen. It learns what happens from events only (CoinCollected,
// LevelRestarted: the pattern of examples/scenes), and reads the level's name and music from the map's
// properties.

#include "game_events.hpp"

#include <nnh/events.hpp>
#include <nnh/tilemap.hpp>

class Hud {
  public:
    /// Listens while its level is on the scene stack (on_enter / on_exit).
    void attach();
    void detach();

    /// Title (map property `title`), `about`, coins and score, the map's `music` property, from y 10 to ~90.
    void draw(const nnh::Tilemap& map, const char* about) const;
    /// The controls, at the bottom.
    void draw_controls() const;

  private:
    void on_coin(const CoinCollected& e);
    void on_restart(const LevelRestarted& e);

    nnh::Subscription coin_sub_;
    nnh::Subscription restart_sub_;
    int coins_ = 0;
    int total_ = 0;
    int score_ = 0;
};
