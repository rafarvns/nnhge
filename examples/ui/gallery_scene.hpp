#pragma once
// The gallery (spec 028 AC-3): a 5-column grid of 48 icons on the bottom screen (four directions, touch), the focused
// icon drawn four times larger on the top screen by the game itself. B goes back.

#include "menu_scene.hpp"

class GalleryScene final : public MenuScene {
public:
    explicit GalleryScene(Demo& demo) : MenuScene(demo, "gallery") {}
    bool create();
    void on_enter() override;

private:
    void step() override;
    void paint() const override;
    nnh::UiWidget grid_;
};
