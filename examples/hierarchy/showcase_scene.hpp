#pragma once
// The hierarchy showcase (spec 022 AC-5): the yard (yard.hpp) played with the buttons and the touch screen. SELECT runs
// the self-check, Y the bench, START quits.

#include "demos.hpp"
#include "yard.hpp"

#include <nnh/scene.hpp>

class ShowcaseScene final : public nnh::Scene {
  public:
    explicit ShowcaseScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void update() override;
    void draw() override;

  private:
    const Demos& demos_;
    yard::Yard yard_;
    bool started_ = false;
    bool ready_ = false;
};
