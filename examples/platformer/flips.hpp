#pragma once
// Flip test: the 8 orientations of an asymmetric tile (Tiled's diagonal, x and y flips) drawn as map tiles,
// the 4 a sprite can do drawn as sprites below them, and a brick wall under a camera moving by fractions of a
// pixel (tiles must never show seams). The console must match the PC screenshot (spec 012 gate).

#include "art.hpp"

#include <nnh/scene.hpp>
#include <nnh/tilemap.hpp>

class Flips final : public nnh::Scene {
  public:
    explicit Flips(const Art& art);
    bool ready() const { return map_.valid(); }

  private:
    void update() override;
    void draw() override;

    const Art& art_;
    nnh::Tilemap map_;
    int arrows_ = -1, wall_ = -1;
    float scroll_ = 0;
};
