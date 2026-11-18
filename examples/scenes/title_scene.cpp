#include "title_scene.hpp"

#include <nnh/debug.hpp>
#include <nnh/input.hpp>

void TitleScene::on_enter() { nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(32, 48, 96)); }

void TitleScene::update() {
    if (nnh::input::pressed(nnh::Button::A)) nnh::scenes::replace(*level_);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop(); // empty stack: the game ends
}

void TitleScene::draw() {
    const int line = nnh::debug::line_height();
    nnh::debug::print(nnh::Screen::Top, 136, 96, "NNHGE  SCENES", nnh::colors::yellow);
    nnh::debug::print(nnh::Screen::Top, 136, 96 + 2 * line, "press A to start");
    nnh::debug::print(nnh::Screen::Bottom, 8, 8, "A      start");
    nnh::debug::print(nnh::Screen::Bottom, 8, 8 + line, "START  quit");
}
