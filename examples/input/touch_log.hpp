#pragma once
// Reacts to TouchStarted / TouchEnded events: counts touches, remembers where the last one began and ended,
// and logs both ("touch started at x, y", "touch ended at x, y") for test scripts. The live position comes
// from the input::touch_position() query instead (no event for moves).

#include <nnh/events.hpp>
#include <nnh/input.hpp>

class TouchLog {
public:
    TouchLog();
    void draw() const;

private:
    void on_started(const nnh::TouchStarted& e);
    void on_ended(const nnh::TouchEnded& e);

    nnh::Subscription started_sub_;
    nnh::Subscription ended_sub_;
    int touches_ = 0;
    nnh::TouchPoint last_start_;
    nnh::TouchPoint last_end_;
};
