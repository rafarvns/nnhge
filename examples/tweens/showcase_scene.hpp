#pragma once
// The showcase: the easing gallery (top) and the intro (bottom), all driven by one TweenSet bound to this
// scene. A pause/resume, B complete, Y restart the intro; X pushes the overlay (this scene's tweens freeze
// under it); SELECT goes to the self-check; START quits.

#include "demos.hpp"
#include "intro.hpp"

#include <nnh/scene.hpp>
#include <nnh/tween.hpp>

class ShowcaseScene final : public nnh::Scene {
public:
    ShowcaseScene(const Demos& demos, bool overlay_at_start)
        : demos_(demos), overlay_at_start_(overlay_at_start) {}

    void on_enter() override;
    void update() override;
    void draw() override;

private:
    const Demos& demos_;
    bool overlay_at_start_;
    nnh::TweenSet tweens_{*this, {.tweens = 16, .steps = 8}}; // updated before update(), cleared on exit
    float phase_ = 0;
    Intro intro_;
};
