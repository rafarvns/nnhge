#pragma once
// The hero (spec 025 AC-4): an A* search (partial paths on) from its cell to the touched goal, the path smoothed into
// waypoints (or not), turned into world points and followed with a PathFollower, the box moved by collision::move at
// 1.5 px per frame. It re-plans when its map's version changes (a door). 1 x 1 (a 12 px box on the agent-1 map) or
// 2 x 2 (26 px on the agent-2 map). Paused (D-Pad Down), it plans but stands still: the script's door lines then come
// from a known cell.

#include <nnh/pathfinding.hpp>

#include "level.hpp"

class Hero {
public:
    static constexpr int kMaxCells = 48 * 30;

    bool init(const Level& level);
    /// Plans from the hero's cell to `goal` and logs "goal: x,y <status> cells n cost c expanded e waypoints w".
    void set_goal(Level& level, nnh::CellPos goal);
    /// Re-plans on a version change ("replan: version v ..."), then walks (unless paused).
    void update(Level& level);
    void set_big(Level& level, bool big);
    void set_smoothing(Level& level, bool on);
    void set_paused(bool paused) { paused_ = paused; }

    bool big() const { return big_; }
    bool smoothing() const { return smoothing_; }
    bool paused() const { return paused_; }
    bool has_goal() const { return has_goal_; }
    nnh::CellPos goal() const { return goal_; }
    nnh::Rect box() const { return box_; }
    nnh::Vec2 center() const { return {box_.x + box_.w / 2, box_.y + box_.h / 2}; }
    nnh::CellPos cell(const Level& level) const { return map(level).cell_at(center()); }
    const nnh::PathMap& map(const Level& level) const { return big_ ? level.big : level.small; }
    const nnh::PathSearcher& searcher() const { return searcher_; }
    nnh::PathResult last() const { return last_; }
    double last_ms() const { return last_ms_; }
    const nnh::CellPos* cells() const { return cells_; }
    int cell_count() const { return cell_count_; }
    const nnh::Vec2* points() const { return points_; }
    int point_count() const { return point_count_; }

private:
    void plan(Level& level, const char* what);

    nnh::PathSearcher searcher_;
    nnh::Rect box_{};
    bool big_ = false;
    bool smoothing_ = true;
    bool paused_ = false;
    bool has_goal_ = false;
    nnh::CellPos goal_{};
    std::uint32_t version_ = 0;
    nnh::PathResult last_{};
    double last_ms_ = 0;
    nnh::CellPos cells_[kMaxCells] = {};
    nnh::CellPos waypoints_[kMaxCells] = {};
    nnh::Vec2 points_[kMaxCells] = {};
    int cell_count_ = 0;
    int point_count_ = 0;
    nnh::PathFollower follower_;
};
