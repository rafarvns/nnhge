#include "play_scene.hpp"

#include <cmath>
#include <cstdio>
#include <utility>

#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL(game); // lines like "I game play: started" in the log and in nnh run (api/log.md)

namespace {

constexpr float kSpeed = 2.0f;    // pixels per frame (the game runs at 60 frames a second)
constexpr float kDeadZone = 0.2f; // Circle Pad values closer to the centre count as rest
constexpr float kSize = 32.0f;    // the sprite's width and height
constexpr float kTopWidth = 400.0f, kTopHeight = 240.0f; // the top screen, in pixels
constexpr int kByeFrames = 30;    // half a second of "Bye!" before the game ends

float clamped(float value, float low, float high) { return value < low ? low : (value > high ? high : value); }

} // namespace

void PlayScene::on_enter() {
    // Assets by path, without the extension (api/assets.md). A failed load logs an error; the game goes on without it.
    if (auto texture = nnh::load_texture("art/player")) player_ = std::move(texture.value());
    else NNH_LOG_ERROR(game, "play: art/player: %s", texture.error().message);
    if (auto sound = nnh::load_sound("sfx/blip")) blip_ = std::move(sound.value());
    else NNH_LOG_ERROR(game, "play: sfx/blip: %s", sound.error().message);

    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(32, 48, 96)); // the screens' background (api/gfx.md)
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(24, 24, 24));

    // Events report changes: the engine publishes SoundFinished when a voice ends (api/events.md).
    sound_done_ = nnh::events::subscribe<&PlayScene::on_sound_finished>(this);
}

void PlayScene::on_exit() { sound_done_.reset(); }

void PlayScene::update() {
    if (!started_) { // on the scene's first frame: a test script is attached by then (tests/smoke.nnhs)
        started_ = true;
        NNH_LOG_INFO(game, "play: started");
    }
    // Quitting: count the "Bye!" frames down, then leave; the empty scene stack ends the game (api/scene.md).
    if (bye_frames_ > 0) {
        if (--bye_frames_ == 0) nnh::scenes::pop();
        return;
    }
    move_player();
    // Buttons are queried each frame: pressed() is true on the one frame a button goes down (api/input.md).
    if (nnh::input::pressed(nnh::Button::A)) {
        blip_voice_ = nnh::audio::play(blip_); // api/audio.md
        NNH_LOG_INFO(game, "play: blip");
    }
    if (nnh::input::pressed(nnh::Button::Start)) {
        NNH_LOG_INFO(game, "play: quit");
        bye_frames_ = kByeFrames;
    }
}

void PlayScene::move_player() {
    // The Circle Pad (its y grows up, the screen's grows down), else the D-Pad (api/input.md).
    const nnh::Stick pad = nnh::input::circle_pad();
    float dx = std::fabs(pad.x) > kDeadZone ? pad.x : 0.0f;
    float dy = std::fabs(pad.y) > kDeadZone ? -pad.y : 0.0f;
    if (nnh::input::held(nnh::Button::Left)) dx = -1;
    if (nnh::input::held(nnh::Button::Right)) dx = 1;
    if (nnh::input::held(nnh::Button::Up)) dy = -1;
    if (nnh::input::held(nnh::Button::Down)) dy = 1;

    position_.x = clamped(position_.x + dx * kSpeed, 0, kTopWidth - kSize); // stay on the top screen
    position_.y = clamped(position_.y + dy * kSpeed, 0, kTopHeight - kSize);
    const bool moving = dx != 0 || dy != 0;
    if (moving_ && !moving) { // one line each time the sprite comes to rest (the test script waits for it)
        NNH_LOG_INFO(game, "play: stopped at (%d, %d)", static_cast<int>(position_.x), static_cast<int>(position_.y));
    }
    moving_ = moving;
}

void PlayScene::on_sound_finished(const nnh::SoundFinished& finished) {
    if (finished.voice == blip_voice_) ++blips_finished_; // the event names the voice that ended
}

void PlayScene::draw() {
    // Text uses the engine's built-in font, the same on every system (api/text.md); sprites: api/gfx.md.
    const nnh::Screen top = nnh::Screen::Top, bottom = nnh::Screen::Bottom;
    nnh::gfx::draw(top, player_, position_);
    nnh::gfx::draw_text(top, {8, 8}, NNH_GAME_TITLE, {.scale = 2, .shadow = true}); // TITLE in CMakeLists.txt
    nnh::gfx::draw_text(top, {8, 226}, "Circle Pad: move   A: sound   START: quit", {.color = nnh::colors::gray});
    if (bye_frames_ > 0) {
        nnh::gfx::draw_text(top, {kTopWidth / 2, 108}, "Bye!", {.scale = 3, .align = nnh::TextAlign::Center});
    }

    char line[48];
    std::snprintf(line, sizeof line, "Sounds finished: %d", blips_finished_); // no heap: a buffer on the stack
    nnh::gfx::draw_text(bottom, {8, 8}, "This is the bottom screen.");
    nnh::gfx::draw_text(bottom, {8, 24}, line, {.color = nnh::colors::yellow});
    nnh::gfx::draw_text(bottom, {8, 200}, "Change src/scenes/play_scene.cpp,\nthen run the game again.",
                        {.color = nnh::colors::gray});
}
