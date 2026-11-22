#pragma once
// The showcase: the gallery of clip modes on the top screen (a library built in code) and the knight on the bottom
// screen (a library loaded from an Aseprite export, driven by its state machine). SELECT: the self-check; START quits.

#include "demos.hpp"
#include "gallery.hpp"
#include "hero.hpp"

#include <nnh/scene.hpp>

class ShowcaseScene final : public nnh::Scene {
public:
    explicit ShowcaseScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

private:
    const Demos& demos_;
    Gallery gallery_;
    Hero hero_;
    bool hero_ok_ = false;
};
