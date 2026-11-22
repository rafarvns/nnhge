#include "play_scene.hpp"

#include "action_log.hpp"

#include <nnh/gfx.hpp>

namespace {
constexpr float kGround = 200;
constexpr float kSize = 16;
constexpr std::uint32_t kFullCharge = 60;
} // namespace

void PlayScene::on_enter() {
    s_.controls.bind_context(Ctx::Play, *this); // enabled while this scene is on top
    pressed_ = nnh::events::subscribe<nnh::ActionPressed>(
        [this](const nnh::ActionPressed& e) { on_edge(e.set, e.action, true); });
    released_ = nnh::events::subscribe<nnh::ActionReleased>(
        [this](const nnh::ActionReleased& e) { on_edge(e.set, e.action, false); });
}

void PlayScene::on_exit() {
    pressed_.reset();
    released_.reset();
}

void PlayScene::on_edge(std::uint16_t set, std::uint16_t action, bool pressed) {
    if (set != s_.controls.id()) return;
    const char* name = s_.controls.name(nnh::ActionId(action));
    char line[64];
    format_edge(line, sizeof line, name, pressed);
    log_line(line);
    if (pressed) s_.check.pressed(name);
}

void PlayScene::update() {
    nnh::ActionSet& c = s_.controls;
    pos_.x += c.vector(Act::Move).x * 2.0f;
    if (pos_.x < kSize) pos_.x = kSize;
    if (pos_.x > 400 - kSize) pos_.x = 400 - kSize;
    if (c.pressed(Act::Jump) && pos_.y >= kGround) vy_ = 5.0f;
    pos_.y -= vy_;
    vy_ -= 0.25f;
    if (pos_.y >= kGround) {
        pos_.y = kGround;
        vy_ = 0;
    }
    if (c.held(Act::Attack)) charge_ = c.held_frames(Act::Attack);
    if (c.released(Act::Attack)) {
        shot_ = {pos_.x, pos_.y - kSize};
        shot_speed_ = 2.0f + 6.0f * static_cast<float>(charge_ < kFullCharge ? charge_ : kFullCharge) / kFullCharge;
        charge_ = 0;
    }
    shot_.x += shot_speed_;
    if (c.pressed(Act::Pause)) nnh::scenes::push(*s_.controls_scene);
}

void PlayScene::draw() {
    const nnh::Screen top = nnh::Screen::Top;
    nnh::gfx::fill_rect(top, {0, kGround + kSize / 2, 400, 240 - kGround}, nnh::rgb(40, 60, 40));
    nnh::gfx::fill_rect(top, {pos_.x - kSize / 2, pos_.y - kSize / 2, kSize, kSize}, nnh::rgb(90, 170, 250));
    const float aim = s_.controls.value(Act::Aim);
    nnh::gfx::draw_line(top, {pos_.x, pos_.y}, {pos_.x + 28, pos_.y - 28 * aim}, nnh::colors::yellow);
    nnh::gfx::fill_rect(top, {shot_.x - 3, shot_.y - 3, 6, 6}, nnh::rgb(250, 120, 60));
    const float charge = static_cast<float>(charge_ < kFullCharge ? charge_ : kFullCharge) / kFullCharge;
    nnh::gfx::draw_rect(top, {8, 8, 102, 10}, nnh::colors::white);
    nnh::gfx::fill_rect(top, {9, 9, 100 * charge, 8}, nnh::rgb(250, 120, 60));
    char line[96];
    s_.controls.describe(Act::Jump, line, sizeof line);
    nnh::gfx::draw_text(top, {120, 8}, "jump:");
    nnh::gfx::draw_text(top, {164, 8}, line);
    s_.controls.describe(Act::Attack, line, sizeof line);
    nnh::gfx::draw_text(top, {120, 20}, "attack:");
    nnh::gfx::draw_text(top, {180, 20}, line);

    const nnh::Screen bottom = nnh::Screen::Bottom;
    const bool touching = s_.controls.held(Act::Attack);
    nnh::gfx::fill_rect(bottom, {kTouchX, kTouchY, kTouchW, kTouchH},
                        touching ? nnh::rgb(250, 120, 60) : nnh::rgb(90, 60, 50));
    nnh::gfx::draw_text(bottom, {kTouchX + kTouchW / 2.0f, kTouchY + 28}, "ATTACK", {.align = nnh::TextAlign::Center});
    nnh::gfx::draw_text(bottom, {8, 120}, "move: Circle Pad / D-Pad\njump, attack: see top\naim: C-Stick (L/R)\nSTART: controls");
    s_.controls.debug_draw(bottom, 8, 8);
}
