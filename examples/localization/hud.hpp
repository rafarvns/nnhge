#pragma once
// The HUD (spec 029): a score and a coin count drawn with the `hud` role's font (a BMFont made by
// art/make_hud_font.py), numbers grouped as the current language writes them.

class Hud {
public:
    void draw(int frame) const;
};
