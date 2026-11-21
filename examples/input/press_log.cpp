#include "press_log.hpp"

NNH_LOG_CHANNEL(demo);

const NamedButton kButtons[14] = {
    {nnh::Button::A, "A"},          {nnh::Button::B, "B"},       {nnh::Button::X, "X"},
    {nnh::Button::Y, "Y"},          {nnh::Button::L, "L"},       {nnh::Button::R, "R"},
    {nnh::Button::ZL, "ZL"},        {nnh::Button::ZR, "ZR"},     {nnh::Button::Start, "START"},
    {nnh::Button::Select, "SELECT"}, {nnh::Button::Up, "UP"},    {nnh::Button::Down, "DOWN"},
    {nnh::Button::Left, "LEFT"},    {nnh::Button::Right, "RIGHT"},
};

const char* button_name(nnh::Button button) {
    for (const NamedButton& b : kButtons) {
        if (b.button == button) return b.name;
    }
    return "?";
}

PressLog::PressLog()
    : pressed_sub_(nnh::events::subscribe<&PressLog::on_pressed>(this)),
      released_sub_(nnh::events::subscribe<&PressLog::on_released>(this)) {}

void PressLog::on_pressed(const nnh::ButtonPressed& e) { NNH_LOG_INFO(demo, "pressed %s", button_name(e.button)); }

void PressLog::on_released(const nnh::ButtonReleased& e) {
    NNH_LOG_INFO(demo, "released %s", button_name(e.button));
}
