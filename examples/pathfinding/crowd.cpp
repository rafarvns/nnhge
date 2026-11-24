#include "crowd.hpp"

#include <nnh/collision.hpp>
#include <utility>

#include "check_maps.hpp"
#include "demos.hpp"

namespace {

constexpr float kSpeed = 1.0f; // px per frame
constexpr float kSide = 10.0f;

} // namespace

bool Crowd::init(const Level& level) {
    auto f = nnh::DistanceField::create({.width = level.width(), .height = level.height()});
    if (!f) {
        NNH_LOG_ERROR(example, "field: %s", f.error().message);
        return false;
    }
    field_ = std::move(f.value());
    const nnh::Rect a = level.crowd_area;
    for (int i = 0; i < kChasers; ++i) { // seeded places on walkable cells of the area
        const std::uint32_t h = pathfinding_demo::hash4(static_cast<std::uint32_t>(i), 77, 0, 5);
        nnh::Vec2 p{a.x + static_cast<float>(h % static_cast<std::uint32_t>(a.w)),
                    a.y + static_cast<float>((h >> 12) % static_cast<std::uint32_t>(a.h))};
        nnh::CellPos c = level.small.cell_at(p);
        for (int k = 0; k < 64 && level.small.blocked(c.x, c.y); ++k) c.x = c.x + 1 < level.width() ? c.x + 1 : 1;
        p = level.small.cell_center(c);
        boxes_[i] = {p.x - kSide / 2, p.y - kSide / 2, kSide, kSide};
    }
    return true;
}

void Crowd::update(Level& level, nnh::CellPos hero) {
    if (!(hero == target_) && level.small.contains(hero)) { // the hero moved on: rebuild toward its new cell
        target_ = hero;
        field_.begin(level.small, &target_, 1);
        frames_ = 0;
    }
    if (field_.building()) {
        ++frames_;
        if (field_.step(kSlice)) {
            NNH_LOG_INFO(example, "field: built %d,%d settled %d frames %d", target_.x, target_.y, field_.settled(),
                         frames_);
        }
    }
    if (!field_.ready()) return;
    const nnh::collision::Grid grid = level.grid();
    for (nnh::Rect& b : boxes_) {
        const nnh::Vec2 c{b.x + b.w / 2, b.y + b.h / 2};
        nnh::Vec2 to{};
        if (!field_.next_point(level.small, c, to)) continue;
        nnh::Vec2 d = to - c;
        const float len2 = d.x * d.x + d.y * d.y;
        if (len2 > kSpeed * kSpeed) d = d * (kSpeed / __builtin_sqrtf(len2));
        const nnh::collision::MoveResult r = nnh::collision::move(grid, b, d);
        b.x = r.position.x;
        b.y = r.position.y;
    }
}
