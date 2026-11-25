#pragma once
// The dialog (spec 028 AC-3): a 9-slice box on the bottom screen whose text appears one character per frame (FR-23);
// A shows it all, then A closes the box. "ui: dialog shown <n>/<total>" is logged once it is fully shown.

#include "menu_scene.hpp"

class DialogScene final : public MenuScene {
public:
    explicit DialogScene(Demo& demo) : MenuScene(demo, "dialog") {}
    bool create();
    void on_enter() override;

private:
    void step() override;
    nnh::UiWidget text_;
    bool shown_ = false;
};
