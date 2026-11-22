#include "demos.hpp"

Demos::Demos(nnh::Scene* const (&scenes)[kCount], Replay& replay) : replay_(replay) {
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
    show(e.demo);
}

void Demos::show(Demo demo) {
    if (demo == current_) return;
    current_ = demo;
    nnh::scenes::replace(*scenes_[static_cast<int>(demo)]);
}
