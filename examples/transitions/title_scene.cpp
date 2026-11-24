#include "title_scene.hpp"

#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/transition.hpp>

#include "demos.hpp"
#include "room_scene.hpp"

namespace tr = nnh::transition;

void TitleScene::on_enter() { NNH_LOG_INFO(example, "transitions: title"); }

void TitleScene::update() {
    if (nnh::input::pressed(nnh::Button::A)) {
        Demos* demos = &demos_;
        tr::replace(*demos_.rooms[0], {.effect = tr::fade(), .out = nnh::Frames{30}, .in = nnh::Frames{30},
                                       .on_covered = [demos] { RoomScene::covered(*demos, true); }});
    }
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void TitleScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 80}, "TRANSITIONS", {.scale = 3, .align = nnh::TextAlign::Center});
    gfx::draw_text(nnh::Screen::Top, {200, 140}, "press A", {.align = nnh::TextAlign::Center});
    gfx::draw_text(nnh::Screen::Bottom, {8, 8},
                   "Fades, wipes, splits, blinds, irises\nand a curtain made of sprites, on one\nor both screens, with "
                   "loading steps\nand a loading scene.");
}

void LoadingScene::on_enter() {
    slices_ = 0;
    leaving_ = false;
    NNH_LOG_INFO(example, "transitions: loading scene");
}

void LoadingScene::update() {
    if (leaving_) return;
    if (slices_ < kSlices) {
        ++slices_;
        return;
    }
    // Loaded: the second transition, to the real room (FR-14).
    Demos* demos = &demos_;
    leaving_ = tr::replace(*demos_.rooms[demos_.loading_target],
                           {.effect = tr::fade(), .out = nnh::Frames{20}, .in = nnh::Frames{20},
                            .on_covered = [demos] { RoomScene::covered(*demos, true); },
                            .on_finished = [demos] { RoomScene::finished(*demos); }});
}

void LoadingScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 90}, "LOADING", {.scale = 2, .align = nnh::TextAlign::Center});
    gfx::draw_text(nnh::Screen::Top, {200, 130}, "tip: a transition can load in slices\nwhile the screen is covered",
                   {.align = nnh::TextAlign::Center});
    demos_.curtain->draw_bar(nnh::Screen::Top, static_cast<float>(slices_) / kSlices, 1);
    demos_.input->look();
}
