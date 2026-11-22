#include "player.hpp"

#include <nnh/input.hpp>

namespace {

// Pixels per frame (60 Hz).
constexpr float kRunSpeed = 1.75f;
constexpr float kGravity = 0.3f;
constexpr float kJumpSpeed = 5.6f;
constexpr float kMaxFall = 6.0f;

} // namespace

PlayerInput read_player_input() {
    using nnh::Button;
    namespace input = nnh::input;
    PlayerInput in;
    if (input::held(Button::Left)) in.run -= 1;
    if (input::held(Button::Right)) in.run += 1;
    in.jump = input::pressed(Button::A) || input::pressed(Button::B);
    in.down = input::held(Button::Down);
    return in;
}

void Player::spawn(nnh::Vec2 feet) {
    box_.x = feet.x - box_.w / 2;
    box_.y = feet.y - box_.h;
    velocity_ = {};
    touching_ = {};
    on_ground_ = false;
}

void Player::update(const nnh::collision::Grid& grid, const PlayerInput& input) {
    const float run = static_cast<float>(input.run);
    velocity_.x = run * kRunSpeed;
    if (input.run != 0) facing_left_ = input.run < 0;

    const bool drop = input.jump && input.down && on_ground_; // through a one-way plank
    if (input.jump && on_ground_ && !drop) velocity_.y = -kJumpSpeed;
    velocity_.y = velocity_.y + kGravity > kMaxFall ? kMaxFall : velocity_.y + kGravity;

    const auto moved = nnh::collision::move(grid, box_, velocity_, {.drop_through = drop});
    box_.x = moved.position.x;
    box_.y = moved.position.y;
    touching_ = moved.touching;
    on_ground_ = moved.touching.floor;
    if (moved.touching.floor || moved.touching.ceiling) velocity_.y = 0;
}
