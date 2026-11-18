#pragma once
// The "game": A collects a coin (published as an event), START pauses, B returns to the title.

#include <nnh/events.hpp>
#include <nnh/input.hpp>
#include <nnh/scene.hpp>

#include "score.hpp"

class LevelScene final : public nnh::Scene {
public:
    LevelScene(nnh::Scene& pause, nnh::Scene& title) : pause_(pause), title_(title) {}

    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

private:
    void on_button(const nnh::ButtonPressed& pressed);

    nnh::Scene& pause_;
    nnh::Scene& title_;
    nnh::Subscription button_sub_;
    Score score_;
    int frames_ = 0;
};
