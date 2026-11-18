#pragma once
// A small side-scrolling world: a critter walks over grass, the top screen's camera follows it, a ghost
// spins in the sky (rotation, tint, opacity), and the bottom screen shows a minimap made of shapes.

#include <nnh/math.hpp>
#include <nnh/texture.hpp>

struct Art {
    nnh::Texture hero;  // 64x16: 4 walking frames of 16x16, facing right
    nnh::Texture tiles; // 64x32: grass tile, dirt tile (32x32 each)
};

class World {
public:
    explicit World(const Art& art) : art_(art) {}
    void update();
    void draw() const;

private:
    void draw_ground() const;
    void draw_sky() const;
    void draw_hud() const;
    void draw_minimap() const;

    const Art& art_;
    nnh::Vec2 hero_{120, 208}; // feet, in world pixels
    bool facing_left_ = false;
    bool walking_ = false;
    float walk_time_ = 0;
    float ghost_angle_ = 0;
};
