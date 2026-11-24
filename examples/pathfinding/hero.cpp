#include "hero.hpp"

#include <nnh/collision.hpp>
#include <nnh/time.hpp>
#include <utility>

#include "demos.hpp"

namespace {

constexpr float kSpeed = 1.5f;  // px per frame
constexpr float kRadius = 3.0f; // the follower's arrival radius (>= the speed)

float box_side(bool big) { return big ? 26.0f : 12.0f; }

} // namespace

bool Hero::init(const Level& level) {
    auto s = nnh::PathSearcher::create(level.width(), level.height());
    if (!s) {
        NNH_LOG_ERROR(example, "searcher: %s", s.error().message);
        return false;
    }
    searcher_ = std::move(s.value());
    const float side = box_side(false);
    box_ = {level.hero_spawn.x - side / 2, level.hero_spawn.y - side / 2, side, side};
    version_ = level.small.version();
    return true;
}

void Hero::plan(Level& level, const char* what) {
    const nnh::PathMap& m = map(level);
    const nnh::Duration start = nnh::time::now();
    last_ = searcher_.find(m, cell(level), goal_, {.partial = true});
    last_ms_ = (nnh::time::now() - start).seconds() * 1000.0;
    version_ = m.version();
    cell_count_ = searcher_.path(cells_, kMaxCells);
    int waypoints = cell_count_;
    const nnh::CellPos* route = cells_;
    if (smoothing_) {
        waypoints = nnh::smooth_path(m, cells_, cell_count_, waypoints_, kMaxCells);
        route = waypoints_;
    }
    point_count_ = nnh::cells_to_points(m, route, waypoints, points_, kMaxCells);
    follower_.reset(points_, point_count_, kRadius);
    if (what[0] == 'g') {
        NNH_LOG_INFO(example, "goal: %d,%d %s cells %d cost %u expanded %d waypoints %d", goal_.x, goal_.y,
                     status_name(last_.status), last_.cells, static_cast<unsigned>(last_.cost), last_.expanded,
                     waypoints);
    } else {
        NNH_LOG_INFO(example, "replan: version %u %s cells %d cost %u expanded %d waypoints %d",
                     static_cast<unsigned>(version_), status_name(last_.status), last_.cells,
                     static_cast<unsigned>(last_.cost), last_.expanded, waypoints);
    }
}

void Hero::set_goal(Level& level, nnh::CellPos goal) {
    goal_ = goal;
    has_goal_ = true;
    plan(level, "goal");
}

void Hero::update(Level& level) {
    if (has_goal_ && map(level).version() != version_) plan(level, "replan");
    if (paused_ || !has_goal_ || follower_.arrived()) return;
    const nnh::Vec2 c = center();
    const nnh::Vec2 t = follower_.target(c);
    nnh::Vec2 d = t - c;
    const float len2 = d.x * d.x + d.y * d.y;
    if (len2 > kSpeed * kSpeed) d = d * (kSpeed / __builtin_sqrtf(len2));
    const nnh::collision::MoveResult r = nnh::collision::move(level.grid(), box_, d);
    box_.x = r.position.x;
    box_.y = r.position.y;
}

void Hero::set_big(Level& level, bool big) {
    big_ = big;
    const nnh::Vec2 c = center();
    const float side = box_side(big);
    box_ = {c.x - side / 2, c.y - side / 2, side, side};
    if (big) { // keep the bigger box out of walls: put it on its anchor's footprint
        const nnh::Vec2 p = level.big.cell_center(level.big.cell_at(c));
        box_ = {p.x - side / 2, p.y - side / 2, side, side};
    }
    NNH_LOG_INFO(example, "hero: %s", big ? "2x2" : "1x1");
    if (has_goal_) plan(level, "replan");
}

void Hero::set_smoothing(Level& level, bool on) {
    smoothing_ = on;
    NNH_LOG_INFO(example, "smoothing: %s", on ? "on" : "off");
    if (has_goal_) plan(level, "replan");
}
