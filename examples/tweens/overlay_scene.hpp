#pragma once
// Pushed over the showcase (X): its own bound set keeps playing while the showcase's set is frozen (only the
// top scene's sets update). START pops it, which clears its set silently.

#include <nnh/gfx.hpp>
#include <nnh/scene.hpp>
#include <nnh/tween.hpp>

class OverlayScene final : public nnh::Scene {
public:
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    nnh::TweenSet tweens_{*this, {.tweens = 4}};
    float panel_y_ = 240, bar_ = 0;
    nnh::Color bar_color_{};
};
