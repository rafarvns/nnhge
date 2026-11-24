#pragma once
// A custom transition effect (spec 023 FR-8) drawn with the example's own sprites: a velvet curtain that falls from
// the top of each screen as the coverage grows, with a gold hem; while the screen is covered by a loading step, a
// progress bar in a frame (the loading progress from the effect's CustomFrame).

#include <nnh/texture.hpp>
#include <nnh/transition.hpp>

class Curtain {
public:
    /// The tileset (art/tiles): cloth, hem and bar frame tiles. Borrowed: keep it alive.
    void set_tiles(const nnh::Texture* tiles) { tiles_ = tiles; }
    /// The effect to put in Options::custom (captures `this`).
    nnh::transition::CustomEffect effect();
    void draw(const nnh::transition::CustomFrame& frame) const;
    /// Draws the loading bar (also used by the loading scene).
    void draw_bar(nnh::Screen screen, float progress, int layer) const;

private:
    const nnh::Texture* tiles_ = nullptr;
};
