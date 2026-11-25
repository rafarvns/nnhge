#pragma once
// The HUD (its own UI: top root only, layer 13, never updated — a UI without input still draws): a health bar anchored
// top-left and a score anchored top-right, changing every second, over a scrolling stripe background the game draws
// with gfx. Every scene draws it.

#include <nnh/ui.hpp>

#include "demos.hpp"

class Hud {
public:
    bool create(const Demo& demo);
    void step(int frame);      ///< the values for this frame (they change once a second)
    void draw(int frame) const; ///< background, then the HUD
    nnh::Ui& ui() { return ui_; }

private:
    nnh::Ui ui_;
    nnh::UiWidget health_, score_, heart_;
    char score_text_[24] = "SCORE 0";
};
