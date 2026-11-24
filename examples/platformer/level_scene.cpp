#include "level_scene.hpp"

#include <nnh/input.hpp>
#include <nnh/time.hpp>
#include <nnh/transition.hpp>

LevelScene::LevelScene(Demo demo, const Art& art, const nnh::Tilemap& map, const char* about, Replay& replay,
                       DoorRun& doors)
    : demo_(demo), map_(map), about_(about), replay_(replay), doors_(doors), play_(art, map, *this) {}

void LevelScene::on_enter() {
    hud_.attach();
    play_.start_at_door(doors_.running());
    play_.restart(); // a level always starts from its spawn point (the door run: left of its door)
    if (nnh::transition::running()) { // came through a door: the iris reopens on the player (spec 023 AC-7)
        const ReplaySample s = play_.sample();
        const nnh::Vec2 centre{s.box.x + s.box.w / 2, s.box.y + s.box.h / 2};
        nnh::transition::reveal_at(centre - s.camera); // a top-screen point (the levels' camera zoom is 1)
    }
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
    if (doors_.running()) {
        play_.update(doors_.input(), true); // the door run walks into the door
        return;
    }
    if (nnh::input::pressed(nnh::Button::Y)) {
        doors_.start(); // ten irises from level to level (door_run.hpp)
        play_.start_at_door(true);
        play_.restart();
        return;
    }
    const PlayerInput input = read_player_input();
    if (replay_.finished() && (input.run != 0 || input.jump)) replay_.dismiss(); // playing again
    play_.update(input, true);
}

void LevelScene::draw() {
    doors_.frame(nnh::time::delta().seconds() * 1000.0); // every frame, transitions included
    play_.draw();
    hud_.draw(map_, about_, play_.entity_count());
    if (replay_.running() || replay_.finished()) {
        replay_.draw_banner();
        replay_.draw_panel(104);
    } else {
        hud_.draw_controls();
    }
}
