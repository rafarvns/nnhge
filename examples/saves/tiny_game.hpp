#pragma once
// The saves example's tiny game (spec 027 AC-5): a square in a world of 32 x 32 rooms, one room per top
// screen (a 20 x 12 grid of 20-pixel tiles). Entering a room marks it explored (and the level grows with the
// rooms explored); each room has a coin (gems + 1) and some an item (into the inventory). Its whole state is
// a Progress, the versioned save type, so storing and loading the game is storing and loading progress().

#include "progress.hpp"

namespace saves_example {

class TinyGame {
  public:
    static constexpr int kWorldSide = 32; ///< rooms per side (kRooms = 32 x 32)
    static constexpr float kSpeed = 2.0f; ///< pixels per frame

    /// A new game: the first name, level 1, room 0, in the middle of the screen.
    void reset();
    /// Moves with the D-Pad or the Circle Pad; play time counts the frames spent moving (so it is the same on
    /// every system for the same inputs).
    void update();
    void draw() const;

    /// The next name of a fixed list (one has a two-byte UTF-8 character). Returns it.
    const char* rename();

    Progress& progress() { return p_; }
    const Progress& progress() const { return p_; }

  private:
    void enter_room(int room);
    void collect();

    Progress p_;
    bool coin_taken_ = false;
    bool item_taken_ = false;
    int name_index_ = 0;
};

} // namespace saves_example
