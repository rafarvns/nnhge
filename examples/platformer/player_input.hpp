#pragma once
// What the player is asked to do on one frame: read from the buttons (player.cpp) or from the replay's script
// (replay_run.hpp), so a scripted run plays exactly like pressing the buttons.

struct PlayerInput {
    int run = 0;       ///< -1 left, 0 stand still, +1 right
    bool jump = false; ///< jump pressed on this frame (A or B)
    bool down = false; ///< Down held: with jump, drops through a one-way plank instead
    bool operator==(const PlayerInput&) const = default;
};
