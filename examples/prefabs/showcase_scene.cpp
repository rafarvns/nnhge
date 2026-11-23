#include "showcase_scene.hpp"

#include "hud.hpp"
#include "room_draw.hpp"

#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace {

constexpr nnh::Vec2 kBottomCamera{40, 0};

} // namespace

void ShowcaseScene::on_enter() {
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {kBottomCamera});
    if (!room_.load()) return;
    room_.restart();
}

void ShowcaseScene::update() {
    namespace input = nnh::input;
    using nnh::Button;
    if (input::pressed(Button::Start)) {
        nnh::scenes::pop();
        return;
    }
    if (input::pressed(Button::Select)) {
        nnh::scenes::replace(*demos_.check);
        return;
    }
    if (input::pressed(Button::Y)) {
        nnh::scenes::replace(*demos_.bench);
        return;
    }
    if (!room_.map().valid()) return;
    if (input::pressed(Button::L)) room_.select(-1);
    if (input::pressed(Button::R)) room_.select(1);
    if (input::pressed(Button::A)) room_.spawn_gold();
    if (input::pressed(Button::X)) room_.restart();
    if (input::touch_started()) {
        const nnh::TouchPoint t = input::touch_position();
        room_.spawn_selected({kBottomCamera.x + static_cast<float>(t.x), kBottomCamera.y + static_cast<float>(t.y)});
    }
    room_.update();
}

void ShowcaseScene::draw() {
    if (!room_.map().valid()) {
        nnh::gfx::draw_text(nnh::Screen::Top, {200, 110}, "rooms/room: see the log", {.align = nnh::TextAlign::Center});
        return;
    }
    room::draw_room(nnh::Screen::Top, room_);
    room::draw_room(nnh::Screen::Bottom, room_);
    nnh::gfx::draw_text(nnh::Screen::Top, {200, 2}, "PREFABS",
                        {.color = nnh::rgb(250, 220, 120), .align = nnh::TextAlign::Center, .shadow = true,
                         .layer = 6});
    room::draw_hud(room_);
}
