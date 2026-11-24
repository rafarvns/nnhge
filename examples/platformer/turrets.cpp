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

void TurretBook::restart() {
    bullets_.clear();
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

float TurretBook::barrel_angle(nnh::Vec2 v) {
    int k = 0; // eighth turns, clockwise from +x on the y-down screen
    if (v.y == 0) {
        k = v.x > 0 ? 0 : 4;
    } else if (v.x == 0) {
        k = v.y > 0 ? 2 : -2;
    } else {
        k = v.x > 0 ? (v.y > 0 ? 1 : -1) : (v.y > 0 ? 3 : -3);
    }
    return static_cast<float>(k) * nnh::kQuarterTurn * 0.5f;
}

nnh::Vec2 TurretBook::muzzle(const nnh::Hierarchy& tree, nnh::Entity barrel) {
    return nnh::transform_point(tree.world_now(barrel).map, kMuzzle);
}

void TurretBook::look(const collision::Grid& grid, nnh::Vec2 target) {
    // Ascending Turret slots = spawn order = the map's object order among turrets (the old book's index order).
    world_.each<level::Turret, level::Body>([&](nnh::Entity e, level::Turret& t, level::Body& body) {
        const nnh::Vec2 eye = centre(body.box);
        const nnh::Vec2 d = target - eye;
        const bool in_range = d.x <= kRangeX && d.x >= -kRangeX && d.y <= kRangeY && d.y >= -kRangeY;
        t.sees = in_range && collision::line_of_sight(grid, eye, target, {.ignore_one_way = true});
        if (t.cooldown > 0) --t.cooldown;
        const nnh::Entity barrel = tree_.first_child(e);
        if (level::Barrel* b = world_.get<level::Barrel>(barrel)) b->lit = t.sees;
        if (t.sees && barrel) tree_.local(barrel)->rotation = barrel_angle(aim(d));
        if (t.sees && t.cooldown == 0) {
            shoot(barrel ? muzzle(tree_, barrel) : eye, aim(d));
            t.cooldown = kFirePeriod;
        }
    });
}

bool TurretBook::shoot(nnh::Vec2 from, nnh::Vec2 velocity) {
    return static_cast<bool>(bullets_.acquire({from, velocity, {}, 0}).handle);
}

void TurretBook::move_bullets(const collision::Grid& grid, nnh::Entity player) {
    const nnh::Storage<level::Body> storage = world_.storage<level::Body>();
    const collision::BoxView bodies =
        collision::box_view(storage.data(), storage.extent(), &level::Body::box, &level::Body::layers);
    for (auto [h, b] : bullets_.items()) {
        if (b.stuck > 0) {
            if (--b.stuck == 0) bullets_.release(h);
            continue;
        }
        const nnh::Vec2 to = b.position + b.velocity;
        collision::RayHit hit;
        // hit_from_inside: the player may have run into the bullet since its last step (both move: a bullet
        // inside the player's box is a hit, not something to ignore).
        const collision::QueryFilter filter{.mask = layers::kBulletMask, .hit_from_inside = true};
        if (!collision::raycast(grid, bodies, b.position, to, hit, filter)) {
            b.position = to;
        } else if (hit.box >= 0 && storage.owner(hit.box) == player) {
            ++hits_;
            flash_ = kFlashFrames;
            bullets_.release(h);
        } else { // a wall: flush on its face
            b.position = hit.point;
            b.normal = hit.normal;
            b.stuck = kStuckFrames;
        }
    }
}

void TurretBook::update(const collision::Grid& grid, nnh::Entity player) {
    bullets_.flush(); // the bullets released last frame are free again (bound: already done by the scene)
    if (flash_ > 0) --flash_;
    const level::Body* body = world_.get<level::Body>(player);
    if (body == nullptr) return;
    look(grid, centre(body->box));
    move_bullets(grid, player);
}

std::uint32_t TurretBook::state() {
    std::uint32_t sum = 2166136261u;
    sum = add_word(sum, static_cast<std::uint32_t>(hits_));
    sum = add_word(sum, static_cast<std::uint32_t>(flash_));
    world_.each<level::Turret>([&](nnh::Entity e, level::Turret& t) {
        sum = add_word(sum, (t.sees ? 0x100u : 0u) | static_cast<std::uint32_t>(t.cooldown));
        if (const nnh::WorldTransform2D* w = tree_.world_transform(tree_.first_child(e))) {
            sum = add_word(sum, bits(w->map.origin.x));
            sum = add_word(sum, bits(w->map.origin.y));
            sum = add_word(sum, bits(w->rotation));
        }
    });
    for (auto [h, b] : bullets_.items()) {
        sum = add_word(sum, bits(b.position.x));
        sum = add_word(sum, bits(b.position.y));
        sum = add_word(sum, static_cast<std::uint32_t>(b.stuck));
    }
    return sum;
}
