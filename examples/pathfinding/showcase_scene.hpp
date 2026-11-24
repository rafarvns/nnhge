#pragma once
// The showcase (spec 025 AC-4): the keep on the top screen through a camera following the hero, the whole level on the
// bottom screen. Touch: the hero's goal. A: the next door; X: hero 1 x 1 / 2 x 2; L: the next diagonal mode; R:
// smoothing; D-Pad Up: the debug view (off, search, field); D-Pad Down: pause the hero; SELECT: self-check; Y: bench;
// START: quit. PATHFINDING_SHOT sets a fixed goal and settings at frame 5 (screenshots).

#include <memory>
#include <nnh/scene.hpp>

#include "crowd.hpp"
#include "debug_view.hpp"
#include "demos.hpp"
#include "hero.hpp"
#include "level.hpp"

class ShowcaseScene final : public nnh::Scene {
public:
    ShowcaseScene(const Demos& demos, Level& level) : demos_(demos), level_(level) {}

    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

private:
    void controls();
    void shot_preset();
    void follow_camera();

    const Demos& demos_;
    Level& level_;
    std::unique_ptr<Hero> hero_; // on the heap: three path buffers
    std::unique_ptr<Crowd> crowd_;
    debug_view::Mode mode_ = debug_view::Mode::Off;
    int next_door_ = 0;
    int frame_ = 0;
    nnh::Vec2 camera_{};
};
