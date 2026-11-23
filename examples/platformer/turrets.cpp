#include "turrets.hpp"

#include <cstring>

namespace {

namespace collision = nnh::collision;

nnh::Vec2 centre(nnh::Rect r) { return {r.x + r.w * 0.5f, r.y + r.h * 0.5f}; }

std::uint32_t add_word(std::uint32_t sum, std::uint32_t word) {
    for (int i = 0; i < 4; ++i) {
        sum ^= (word >> (8 * i)) & 0xFFu;
        sum *= 16777619u;
    }
    return sum;
}

std::uint32_t bits(float v) {
    std::uint32_t b;
    std::memcpy(&b, &v, sizeof b);
    return b;
}

} // namespace

void TurretBook::clear() {
    count_ = 0;
    restart();
}

bool TurretBook::add(nnh::Rect box) {
    if (count_ >= kMaxTurrets) return false;
    turrets_[count_++] = {box, false, 0};
    return true;
}

void TurretBook::restart() {
    for (Turret& t : turrets_) {
        t.sees = false;
        t.cooldown = 0;
    }
    for (Bullet& b : bullets_) b = {};
    hits_ = 0;
    flash_ = 0;
}

nnh::Vec2 TurretBook::aim(nnh::Vec2 d) {
    const float ax = d.x < 0 ? -d.x : d.x;
    const float ay = d.y < 0 ? -d.y : d.y;
    const float sx = d.x < 0 ? -1.0f : 1.0f;
    const float sy = d.y < 0 ? -1.0f : 1.0f;
    if (ay * 5 < ax * 2) return {2 * sx, 0};      // within about 22 degrees of horizontal
    if (ax * 5 < ay * 2) return {0, 2 * sy};      // of vertical
    return {1.375f * sx, 1.375f * sy};            // a diagonal (about 1.94 px per frame)
}

void TurretBook::look(const collision::Grid& grid, nnh::Vec2 target) {
    for (int i = 0; i < count_; ++i) {
        Turret& t = turrets_[i];
        const nnh::Vec2 eye = centre(t.box);
        const nnh::Vec2 d = target - eye;
        const bool in_range = d.x <= kRangeX && d.x >= -kRangeX && d.y <= kRangeY && d.y >= -kRangeY;
        t.sees = in_range && collision::line_of_sight(grid, eye, target, {.ignore_one_way = true});
        if (t.cooldown > 0) --t.cooldown;
        if (t.sees && t.cooldown == 0) {
            shoot(eye, aim(d));
            t.cooldown = kFirePeriod;
        }
    }
}

bool TurretBook::shoot(nnh::Vec2 from, nnh::Vec2 velocity) {
    for (Bullet& b : bullets_) {
        if (b.active) continue;
        b = {from, velocity, {}, 0, true};
        return true;
    }
    return false;
}

void TurretBook::move_bullets(const collision::Grid& grid) {
    const collision::BoxView bodies = collision::box_view(bodies_, 1 + count_, &Body::box, &Body::layers);
    for (Bullet& b : bullets_) {
        if (!b.active) continue;
        if (b.stuck > 0) {
            b.active = --b.stuck > 0;
            continue;
        }
        const nnh::Vec2 to = b.position + b.velocity;
        collision::RayHit hit;
        if (!collision::raycast(grid, bodies, b.position, to, hit, {.mask = layers::kBulletMask})) {
            b.position = to;
        } else if (hit.box == 0) { // the player
            ++hits_;
            flash_ = kFlashFrames;
            b.active = false;
        } else { // a wall: flush on its face
            b.position = hit.point;
            b.normal = hit.normal;
            b.stuck = kStuckFrames;
        }
    }
}

void TurretBook::update(const collision::Grid& grid, nnh::Rect player) {
    if (flash_ > 0) --flash_;
    bodies_[0] = {player, layers::kPlayer};
    for (int i = 0; i < count_; ++i) bodies_[1 + i] = {turrets_[i].box, layers::kEnemy};
    look(grid, centre(player));
    move_bullets(grid);
}

std::uint32_t TurretBook::state() const {
    std::uint32_t sum = 2166136261u;
    sum = add_word(sum, static_cast<std::uint32_t>(hits_));
    sum = add_word(sum, static_cast<std::uint32_t>(flash_));
    for (int i = 0; i < count_; ++i) {
        sum = add_word(sum, (turrets_[i].sees ? 0x100u : 0u) | static_cast<std::uint32_t>(turrets_[i].cooldown));
    }
    for (const Bullet& b : bullets_) {
        if (!b.active) continue;
        sum = add_word(sum, bits(b.position.x));
        sum = add_word(sum, bits(b.position.y));
        sum = add_word(sum, static_cast<std::uint32_t>(b.stuck));
    }
    return sum;
}
