#include "level_scene.hpp"

#include <nnh/input.hpp>

LevelScene::LevelScene(Demo demo, const Art& art, const nnh::Tilemap& map, const char* about, Replay& replay)
    : demo_(demo), map_(map), about_(about), replay_(replay), play_(art, map, *this) {}

void LevelScene::on_enter() {
    hud_.attach();
    play_.restart(); // a level always starts from its spawn point
}

void LevelScene::on_exit() {
    hud_.detach();
}

void LevelScene::update() {
    const Replay::Drive drive = replay_.drive(demo_);
    if (drive.active) {
        if (drive.frozen) return;
        if (drive.restart) play_.restart();
        play_.update(drive.input, false);
        replay_.record(demo_, play_.sample());
        return;
    }
    if (nnh::input::pressed(nnh::Button::X)) {
        replay_.start(); // drives this level (or asks for level 1) from the next frame
        return;
    }
    const PlayerInput input = read_player_input();
    if (replay_.finished() && (input.run != 0 || input.jump)) replay_.dismiss(); // playing again
    play_.update(input, true);
}

void LevelScene::draw() {
    play_.draw();
    hud_.draw(map_, about_);
    if (replay_.running() || replay_.finished()) {
        replay_.draw_banner();
        replay_.draw_panel(104);
    } else {
        hud_.draw_controls();
    }
}
