#include "pause_scene.hpp"

#include <nnh/debug.hpp>
#include <nnh/input.hpp>

void PauseScene::update() {
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop(); // back to the level
}

void PauseScene::draw() {
    const int line = nnh::debug::line_height();
    nnh::debug::print(nnh::Screen::Top, 160, 112, "- PAUSED -", nnh::colors::yellow);
    nnh::debug::print(nnh::Screen::Bottom, 8, 8 + 6 * line, "PAUSED: START resumes", nnh::colors::yellow);
}
