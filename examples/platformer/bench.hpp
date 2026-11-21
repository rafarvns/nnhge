#pragma once
// Benchmark for the spec 012 gate (decision D26): N full layers of 16x16 tiles scrolling diagonally on the
// top screen (every cell drawn: up to 26 x 16 = 416 tiles per layer) plus M sprites. L/R change N, Y/X change
// M. The bottom screen shows the frame time and the frames that missed 60 Hz since the last change.

#include "art.hpp"

#include <cstdint>
#include <memory>
#include <nnh/tilemap.hpp>

class Bench {
  public:
    static constexpr int kMaxLayers = 12;
    static constexpr int kMaxSprites = 2000;
    static constexpr int kMaxTiles = kMaxLayers * 26 * 16; ///< for AppConfig::gfx.max_tiles

    /// Builds its map (kMaxLayers full layers on `art.tiles`). Starts with 4 layers and 300 sprites (the
    /// gate).
    explicit Bench(const Art& art);
    bool ready() const { return map_.valid() && sprites_ != nullptr; }
    void update();
    void draw() const;

  private:
    struct Sprite {
        float x, y, vx, vy;
    };

    void set_counts(int layers, int sprites);

    const Art& art_;
    nnh::Tilemap map_;
    std::unique_ptr<Sprite[]> sprites_; // kMaxSprites, allocated once (32 KB: too big for the 3DS stack)
    int layer_count_ = 4;
    int sprite_count_ = 300;
    nnh::Vec2 camera_{3, 5};
    nnh::Vec2 camera_speed_{1.3f, 0.7f};
    // Frame times since the counts last changed.
    double average_ms_ = 0;
    double worst_ms_ = 0;
    int frames_ = 0;
    int missed_ = 0; ///< frames that took longer than 1.5 x 16.7 ms
};
