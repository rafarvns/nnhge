#pragma once
// Benchmark: many 16x16 sprites from one texture bouncing on the top screen, to measure how many the
// system draws at 60 Hz (spec 004 AC-3). L/R change the count by 100, X/Y by 1000.

#include <nnh/texture.hpp>

class Bench {
public:
    static constexpr int kMaxSprites = 8000;

    explicit Bench(const nnh::Texture& hero, int count = 1000);
    void update();
    void draw() const;

private:
    void set_count(int count);

    const nnh::Texture& hero_;
    int count_ = 0;
    double frame_ms_ = 0; // average frame time (smoothed)
    float x_[kMaxSprites];
    float y_[kMaxSprites];
    float vx_[kMaxSprites];
    float vy_[kMaxSprites];
};
