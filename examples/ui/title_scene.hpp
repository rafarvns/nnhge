#pragma once
// The title (spec 028 AC-3): a centred VBox of New game, Continue (disabled: skipped), Options, Inventory, Gallery,
// Extras (disabled but inert: refused) and Quit on the bottom screen; the game's name and a hint on the top. New game
// opens the dialog, Options/Inventory/Gallery push their screens, Quit asks "Quit the game?" (No by default, cancel =
// No; Yes ends the program).

#include "menu_scene.hpp"

class TitleScene final : public MenuScene {
public:
    explicit TitleScene(Demo& demo) : MenuScene(demo, "title") {}
    bool create();
    void on_enter() override;

private:
    void step() override;
    nnh::UiWidget menu_;
    nnh::UiWidget prompt_;
};
