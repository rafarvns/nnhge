#pragma once
// The crowd (spec 025 AC-4): 32 chasers sharing one double-buffered distance field toward the hero's cell, rebuilt in
// slices of 512 cells per frame whenever the hero enters another cell (the last finished field keeps answering
// meanwhile); each chaser walks toward next_point() with collision::move. Logs "field: built x,y settled n frames f".

#include <nnh/pathfinding.hpp>

#include "level.hpp"

class Crowd {
public:
    static constexpr int kChasers = 32;
    static constexpr int kSlice = 512;

    bool init(const Level& level);
    void update(Level& level, nnh::CellPos hero);
    const nnh::DistanceField& field() const { return field_; }
    const nnh::Rect* boxes() const { return boxes_; }
    nnh::CellPos target() const { return target_; }

private:
    nnh::DistanceField field_;
    nnh::Rect boxes_[kChasers] = {};
    nnh::CellPos target_{-1, -1};
    int frames_ = 0;
};
