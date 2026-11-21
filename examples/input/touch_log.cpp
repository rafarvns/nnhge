#include "touch_log.hpp"

#include <nnh/debug.hpp>

#include "press_log.hpp"

TouchLog::TouchLog()
    : started_sub_(nnh::events::subscribe<&TouchLog::on_started>(this)),
      ended_sub_(nnh::events::subscribe<&TouchLog::on_ended>(this)) {}

void TouchLog::on_started(const nnh::TouchStarted& e) {
    ++touches_;
    last_start_ = e.position;
    NNH_LOG_INFO(demo, "touch started at %d, %d", e.position.x, e.position.y);
}

void TouchLog::on_ended(const nnh::TouchEnded& e) {
    last_end_ = e.position;
    NNH_LOG_INFO(demo, "touch ended at %d, %d", e.position.x, e.position.y);
}

void TouchLog::draw() const {
    const int line = nnh::debug::line_height();
    const nnh::Screen bottom = nnh::Screen::Bottom;
    if (nnh::input::touching()) {
        const nnh::TouchPoint p = nnh::input::touch_position();
        nnh::debug::print(bottom, p.x - 4, p.y - line / 2, "+", nnh::colors::green);
        nnh::debug::printf(bottom, 8, 8 + line, "touching at %d, %d", p.x, p.y);
    } else {
        nnh::debug::print(bottom, 8, 8 + line, "not touching", nnh::colors::gray);
    }
    nnh::debug::printf(bottom, 8, 8 + 2 * line, "touches %d  last %d,%d -> %d,%d", touches_, last_start_.x,
                       last_start_.y, last_end_.x, last_end_.y);
}
