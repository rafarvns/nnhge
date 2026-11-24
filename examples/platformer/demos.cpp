#include "demos.hpp"

#include "door_transition.hpp"

#include <nnh/gfx.hpp>
#include <nnh/transition.hpp>

Demos::Demos(nnh::Scene* const (&scenes)[kCount], Replay& replay, DoorRun& doors)
    : replay_(replay), doors_(doors) {
    for (int i = 0; i < kCount; ++i)
        scenes_[i] = scenes[i];
    button_sub_ = nnh::events::subscribe<&Demos::on_button>(this);
    show_sub_ = nnh::events::subscribe<&Demos::on_show>(this);
}

nnh::Scene& Demos::first(Demo demo) {
    current_ = demo;
    return *scenes_[static_cast<int>(demo)];
}

void Demos::on_button(const nnh::ButtonPressed& e) {
    if (e.button == nnh::Button::Start) {
        nnh::scenes::pop(); // the only scene: nnh::run() returns
    } else if (e.button == nnh::Button::Select) {
        replay_.stop(); // leaving by hand: no verdict
        replay_.dismiss();
        show(static_cast<Demo>((static_cast<int>(current_) + 1) % kCount));
    }
}

void Demos::on_show(const ShowDemo& e) {
    if (e.door && !replay_.running()) {
        door(e);
    } else {
        show(e.demo);
    }
}

void Demos::show(Demo demo) {
    if (demo == current_) return;
    current_ = demo;
    nnh::scenes::replace(*scenes_[static_cast<int>(demo)]);
}

void Demos::door(const ShowDemo& e) {
    if (e.demo == current_) return;
    // The iris closes on the player's top-screen point, from the camera still in place (spec 023 AC-7).
    const nnh::Camera2D camera = nnh::gfx::camera(nnh::Screen::Top);
    nnh::transition::Options options = door_transition((e.at - camera.position) * camera.zoom);
    DoorRun* doors = &doors_;
    options.on_finished = [doors] { doors->door_done(); };
    if (!nnh::transition::replace(*scenes_[static_cast<int>(e.demo)], options)) return; // a second door this frame
    doors_.door(current_, e.demo);
    current_ = e.demo;
}
