#pragma once
// A level as a scene (spec 001): level 1 (the Tiled map) and the bonus level (built in code) are two
// LevelScene objects. The player plays it with the buttons, or the replay drives it (replay.hpp); X starts
// the replay.

#include "hud.hpp"
#include "level_play.hpp"
#include "replay.hpp"

#include <nnh/scene.hpp>

class LevelScene final : public nnh::Scene {
  public:
    /// `about`: one line under the title. Everything passed in must outlive the scene.
    LevelScene(Demo demo, const Art& art, const nnh::Tilemap& map, const char* about, Replay& replay);

  private:
    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

    Demo demo_;
    const nnh::Tilemap& map_;
    const char* about_;
    Replay& replay_;
    LevelPlay play_;
    Hud hud_;
};
