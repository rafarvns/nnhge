#pragma once
// The inventory (spec 028 AC-3): a list of 300 items on the bottom screen (icon, name, count; L/R page; the row pool
// asks the game for rows only as they come into view) and the focused item's name and description in a 9-slice panel
// on the top screen. B goes back.

#include "menu_scene.hpp"

class InventoryScene final : public MenuScene {
public:
    explicit InventoryScene(Demo& demo) : MenuScene(demo, "inventory") {}
    bool create();
    void on_enter() override;

private:
    void step() override;
    void describe(int item);
    nnh::UiWidget list_, name_, text_;
};
