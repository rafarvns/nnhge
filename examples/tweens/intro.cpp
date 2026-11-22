#include "intro.hpp"

using nnh::Duration;
using nnh::Ease;

void Intro::start(nnh::TweenSet& set) {
    set_ = &set;
    set.kill(sequence_);
    set.kill(ready_timer_);
    ready_ = false;
    // from_to everywhere: the first group writes its start values at once, so the first frame is already
    // right.
    sequence_ =
        set.sequence()
            .from_to(&panel_y_, 250.0f, 70.0f, Duration::from_ms(500), {.ease = Ease::BackOut})
            .join()
            .from_to(&panel_, nnh::rgb(20, 20, 40), nnh::rgb(30, 120, 140), Duration::from_ms(500))
            .from_to(&title_x_, -200.0f, 160.0f, Duration::from_ms(700), {.ease = Ease::ElasticOut})
            .wait(Duration::from_ms(200))
            .call([this] { ready_timer_ = set_->after(Duration::from_ms(500), [this] { ready_ = true; }); });
}

void Intro::update() {
    if (blink_.update() % 2 == 1) blink_on_ = !blink_on_; // update() returns how many periods ended
}

void Intro::draw() const {
    const nnh::Screen bottom = nnh::Screen::Bottom;
    nnh::gfx::fill_rect(bottom, {40, panel_y_, 240, 100}, panel_);
    nnh::gfx::draw_text(bottom, {title_x_, panel_y_ + 16}, "NNHGE TWEENS",
                        {.scale = 2, .align = nnh::TextAlign::Center, .shadow = true});
    if (ready_ && blink_on_) {
        nnh::gfx::draw_text(bottom, {160, panel_y_ + 60}, "READY",
                            {.color = nnh::colors::yellow, .scale = 2, .align = nnh::TextAlign::Center});
    }
}
