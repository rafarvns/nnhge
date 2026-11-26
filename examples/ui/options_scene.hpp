#pragma once
// The options screen (spec 028 AC-3): a scroll box of Music and Sound sliders (0-10), Invert Y, Difficulty, Text speed
// and Language choices, Online (disabled but inert: refused), six more toggles so the box scrolls, and Back (the box's
// back widget: B activates it). Slid in by a spec 015 tween through set_render_offset as a setter. Changing the
// language switches every UI's resolver (text_changed).

#include <nnh/tween.hpp>

#include "menu_scene.hpp"

class OptionsScene final : public MenuScene {
public:
    explicit OptionsScene(Demo& demo) : MenuScene(demo, "options"), tweens_(*this, {.tweens = 2}) {}
    bool create();
    void on_enter() override;
    nnh::UiWidget music() const { return music_; }
    nnh::UiWidget panel() const { return panel_; }

private:
    void step() override;
    nnh::TweenSet tweens_;
    nnh::UiWidget panel_, box_, music_, back_;
};
