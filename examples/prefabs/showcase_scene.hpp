#pragma once
// The showcase (spec 021 AC-5): the room on the top screen; the bottom screen shows it through a Camera2D at (40, 0),
// so a touch at (x, y) is the world point (40 + x, y). A touch spawns the selected prefab there ("spawned: ..." or
// "missed: ..."), L/R cycle the selection ("selected: ..."), A spawns a gold coin by override at the room's centre,
// X restarts the room (clear() + the map's objects: the same "map: ..." line), SELECT the self-check, Y the bench,
// START quits.

#include "demos.hpp"
#include "room.hpp"

class ShowcaseScene final : public nnh::Scene {
  public:
    explicit ShowcaseScene(const Demos& demos) : demos_(demos), room_(*this) {}

  private:
    void on_enter() override;
    void update() override;
    void draw() override;

    const Demos& demos_;
    room::Room room_;
};
