#include "player.hpp"

#include <nnh/input.hpp>
#include <nnh/texture.hpp>

namespace {

// Pixels per frame (60 Hz).
constexpr float kRunSpeed = 1.75f;
constexpr float kGravity = 0.3f;
constexpr float kJumpSpeed = 5.6f;
constexpr float kMaxFall = 6.0f;

} // namespace

void Player::spawn(nnh::Vec2 feet) {
    box_.x = feet.x - box_.w / 2;
    box_.y = feet.y - box_.h;
    velocity_ = {};
    on_ground_ = false;
}

void Player::update(const nnh::collision::Grid& grid) {
    using nnh::Button;
    namespace input = nnh::input;
    float run = 0;
    if (input::held(Button::Left)) run -= 1;
    if (input::held(Button::Right)) run += 1;
    velocity_.x = run * kRunSpeed;
    if (run != 0) facing_left_ = run < 0;
    walk_frames_ = run != 0 && on_ground_ ? walk_frames_ + 1 : 0;

    const bool jump = input::pressed(Button::A) || input::pressed(Button::B);
    const bool drop = jump && input::held(Button::Down) && on_ground_; // through a one-way plank
    if (jump && on_ground_ && !drop) velocity_.y = -kJumpSpeed;
    velocity_.y = velocity_.y + kGravity > kMaxFall ? kMaxFall : velocity_.y + kGravity;

    const auto moved = nnh::collision::move(grid, box_, velocity_, {.drop_through = drop});
    box_.x = moved.position.x;
    box_.y = moved.position.y;
    on_ground_ = moved.touching.floor;
    if (moved.touching.floor || moved.touching.ceiling) velocity_.y = 0;
}

void Player::draw(nnh::Screen screen, const nnh::Texture& sheet, int layer) const {
    int frame = 0;
    if (!on_ground_) {
        frame = 3;
    } else if (walk_frames_ > 0) {
        frame = 1 + (walk_frames_ / 8) % 2;
    }
    nnh::gfx::draw(screen, sheet, {box_.x - 3, box_.y - 2},
                   {.region = {frame * 16, 0, 16, 16}, .flip_x = facing_left_, .layer = layer});
}
