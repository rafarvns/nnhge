#include "level_scene.hpp"

#include <nnh/debug.hpp>

void LevelScene::on_enter() {
    score_.start();
    frames_ = 0;
    button_sub_ = nnh::events::subscribe<&LevelScene::on_button>(this);
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(16, 64, 32));
}

void LevelScene::on_exit() {
    button_sub_.reset();
    score_.stop();
}

// Reacting to moments through events...
void LevelScene::on_button(const nnh::ButtonPressed& pressed) {
    if (nnh::scenes::top() != this) return; // under the pause menu: ignore buttons
    switch (pressed.button) {
    case nnh::Button::A: nnh::events::publish(CoinCollected{10}); break;
    case nnh::Button::Start: nnh::scenes::push(pause_); break;
    case nnh::Button::B: nnh::scenes::replace(title_); break;
    default: break;
    }
}

// ...and to continuous state by querying (only runs while on top).
void LevelScene::update() {
    ++frames_;
    if (nnh::input::held(nnh::Button::Y) && frames_ % 10 == 0) nnh::events::publish(CoinCollected{1});
}

void LevelScene::draw() {
    const int line = nnh::debug::line_height();
    nnh::debug::print(nnh::Screen::Top, 8, 8, "LEVEL", nnh::colors::white);
    nnh::debug::printf(nnh::Screen::Top, 8, 8 + 2 * line, "Score: %d  (%d coins)", score_.value(), score_.coins());
    nnh::debug::printf(nnh::Screen::Top, 8, 8 + 3 * line, "Time:  %d.%d s", frames_ / 60, frames_ % 60 / 6);
    nnh::debug::print(nnh::Screen::Bottom, 8, 8, "A      coin (+10)");
    nnh::debug::print(nnh::Screen::Bottom, 8, 8 + line, "Y held coin (+1)");
    nnh::debug::print(nnh::Screen::Bottom, 8, 8 + 2 * line, "START  pause");
    nnh::debug::print(nnh::Screen::Bottom, 8, 8 + 3 * line, "B      back to title");
}
