#pragma once
// The bottom screen of the showcase: a small intro built as one sequence (a panel slides in with BackOut
// while its colour fades, the title springs in, a short wait, then a timer shows "READY"), and a blink driven
// by the plain nnh::Timer value type (polled, no callback).

#include <nnh/gfx.hpp>
#include <nnh/tween.hpp>

class Intro {
public:
    /// Starts the intro in `set` (the showcase's bound set). Calling it again restarts from the beginning.
    void start(nnh::TweenSet& set);
    void update(); ///< the blink timer
    void draw() const;

    nnh::TweenHandle handle() const { return sequence_; }

private:
    nnh::TweenSet* set_ = nullptr;
    nnh::TweenHandle sequence_{};
    nnh::TweenHandle ready_timer_{};
    float panel_y_ = 250, title_x_ = -200;
    nnh::Color panel_{};
    bool ready_ = false;
    bool blink_on_ = true;
    nnh::Timer blink_ = nnh::Timer::repeating(nnh::Duration::from_ms(400));
};
