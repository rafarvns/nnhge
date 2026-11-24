#include "input_log.hpp"

#include <nnh/log.hpp>
#include <nnh/transition.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

const char* button_name(nnh::Button button) {
    switch (button) {
    case nnh::Button::A: return "A";
    case nnh::Button::B: return "B";
    case nnh::Button::X: return "X";
    case nnh::Button::Y: return "Y";
    case nnh::Button::L: return "L";
    case nnh::Button::R: return "R";
    case nnh::Button::ZL: return "ZL";
    case nnh::Button::ZR: return "ZR";
    case nnh::Button::Start: return "START";
    case nnh::Button::Select: return "SELECT";
    case nnh::Button::Up: return "UP";
    case nnh::Button::Down: return "DOWN";
    case nnh::Button::Left: return "LEFT";
    case nnh::Button::Right: return "RIGHT";
    }
    return "?";
}

void InputLog::attach() {
    buttons_ = nnh::events::subscribe<&InputLog::on_button>(this);
    touches_ = nnh::events::subscribe<&InputLog::on_touch>(this);
}

void InputLog::detach() {
    buttons_.reset();
    touches_.reset();
}

void InputLog::on_button(const nnh::ButtonPressed& e) {
    if (nnh::transition::running()) ++seen_;
    NNH_LOG_INFO(example, "input: %s", button_name(e.button));
}

void InputLog::on_touch(const nnh::TouchStarted& e) {
    if (nnh::transition::running()) ++seen_;
    NNH_LOG_INFO(example, "input: touch %d,%d", e.position.x, e.position.y);
}

void InputLog::look() {
    // From the transition's first frame (the frame of the request still read the press that started it).
    if (nnh::transition::frames() == 0) return;
    const nnh::Stick pad = nnh::input::circle_pad();
    if (nnh::input::held_mask() != 0 || nnh::input::touching() || pad.x != 0 || pad.y != 0) ++seen_;
}
