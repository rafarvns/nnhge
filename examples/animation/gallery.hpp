#pragma once
// The top screen of the showcase: one library built in code (the numbered strip art/gallery.png) and six animators
// playing it as loop, once, repeat 3, ping-pong, ping-pong reverse x2 and reverse, each with its position, loops and
// finished flag; L/R change their speed (0, 0.5, 1, 1.5, 2), Y restarts them; `tick` markers flash the cell.

#include <nnh/animation.hpp>
#include <nnh/texture.hpp>

#include "libraries.hpp"

class Gallery {
public:
    void load();   ///< the texture and the library
    void unload(); ///< before the engine shuts down
    void update(); ///< input (L/R, Y) and one update of every animator
    void draw() const;

private:
    void restart();

    nnh::Texture texture_;
    nnh::SpriteLibrary library_;
    nnh::SpriteAnimator cells_[kGalleryClips];
    int flash_[kGalleryClips] = {};
    int speed_ = 2; ///< index in the speeds
};
