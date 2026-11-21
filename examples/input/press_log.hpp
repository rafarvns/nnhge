#pragma once
// Logs every press and release ("pressed A", "released A") from the ButtonPressed / ButtonReleased events, so
// a test script can expect them (tests/remote.nnhs, spec 013). Also the example's button names and its log
// channel (`demo`), shared with touch_log.cpp.

#include <nnh/events.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(demo);

struct NamedButton {
    nnh::Button button;
    const char* name;
};

/// Every button, in the order the example shows them.
extern const NamedButton kButtons[14];

const char* button_name(nnh::Button button);

class PressLog {
public:
    PressLog();

private:
    void on_pressed(const nnh::ButtonPressed& e);
    void on_released(const nnh::ButtonReleased& e);

    nnh::Subscription pressed_sub_;
    nnh::Subscription released_sub_;
};
