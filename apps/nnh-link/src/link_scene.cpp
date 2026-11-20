#include "link_scene.hpp"

#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

#include "storage_file.hpp"
#include "views.hpp"

namespace link_app {

using nnh::Button;
using nnh::input::pressed;

void LinkScene::on_enter() {
    nnh::gfx::set_clear_color(nnh::Screen::Top, view::kTopBackground);
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, view::kBottomBackground);
    nnh::input::set_circle_pad_mirrors_dpad(true);
}

void LinkScene::press_pad_key(char c) {
    if (c == Pad::kErase) {
        link_.erase();
    } else if (c == Pad::kOk) {
        link_.press(Key::A);
    } else if (c != 0) {
        link_.type(c);
    }
}

void LinkScene::pad_input() {
    if (pad_screen_ != link_.screen()) { // a fresh pad for each visit
        pad_ = Pad(link_.screen() == Screen::Address);
        pad_screen_ = link_.screen();
    }
    if (pressed(Button::Up)) pad_.move(0, -1);
    if (pressed(Button::Down)) pad_.move(0, 1);
    if (pressed(Button::Left)) pad_.move(-1, 0);
    if (pressed(Button::Right)) pad_.move(1, 0);
    if (pressed(Button::A)) press_pad_key(pad_.focused());
    if (pressed(Button::Y)) link_.erase();
    if (pressed(Button::B)) link_.press(Key::B);
    if (nnh::input::touch_started()) {
        const nnh::TouchPoint t = nnh::input::touch_position();
        press_pad_key(pad_.hit(t.x, t.y));
    }
}

void LinkScene::list_input() {
    static constexpr struct {
        Button button;
        Key key;
    } kKeys[] = {{Button::Up, Key::Up},     {Button::Down, Key::Down}, {Button::A, Key::A}, {Button::B, Key::B},
                 {Button::X, Key::X},       {Button::Y, Key::Y},       {Button::Start, Key::Start}};
    for (const auto& k : kKeys) {
        if (pressed(k.button)) link_.press(k.key);
    }
    if (!nnh::input::touch_started()) return;
    const nnh::TouchPoint t = nnh::input::touch_position();
    const Screen screen = link_.screen();
    if (screen == Screen::Servers || screen == Screen::Games) {
        const int rows = screen == Screen::Servers ? static_cast<int>(link_.servers().size()) + 1
                                                   : static_cast<int>(link_.games().size());
        link_.choose(layout::row_at(t.y, link_.selected(), rows));
    } else if (layout::in_button(t.x, t.y)) {
        link_.press(screen == Screen::Connecting || screen == Screen::Building ? Key::B : Key::A);
    }
}

void LinkScene::update() {
    const Screen screen = link_.screen();
    if (screen == Screen::Code || screen == Screen::Address) {
        pad_input();
    } else {
        pad_screen_ = screen;
        list_input();
    }
    link_.tick();
    if (persist_) save_storage(storage_);
    if (link_.quit()) nnh::scenes::pop();
}

void LinkScene::draw() {
    switch (link_.screen()) {
    case Screen::Servers: view::draw_servers(link_); break;
    case Screen::Games: view::draw_games(link_); break;
    case Screen::Address:
    case Screen::Code: view::draw_pad(link_, pad_); break;
    case Screen::Connecting:
    case Screen::Building: view::draw_progress(link_); break;
    case Screen::Done:
    case Screen::Message: view::draw_message(link_); break;
    }
}

} // namespace link_app
