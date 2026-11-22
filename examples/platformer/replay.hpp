#pragma once
// The replay (spec 012 T7.3, FR-10): X in a level (or PLATFORMER_DEMO=replay) plays the scripted run of
// replay_script.cpp, level 1 then the bonus level, with no buttons. It logs a checksum of the player and the
// camera every 60 frames (`replay: 60 level1 x 121 y 242 coins 1 sum 0CF7E4CE`), so the PC's and the
// console's logs can be compared line by line, and judges 60 Hz in-game: a frame over 25 ms missed its slot.
// The last line has the final checksum and the verdict (`replay: done 900 frames sum 26B21D57: 60 Hz PASS
// (...)`, or `60 Hz FAIL` as a warning); the verdict is on both screens too.
//
// The levels ask it each frame whether it drives them (drive()) and hand it what the frame left (record()).
// It switches levels by publishing ShowDemo.

#include "game_events.hpp"
#include "replay_run.hpp"

class Replay {
  public:
    /// What a level does this frame.
    struct Drive {
        bool active = false;  ///< the replay is running: real buttons are ignored
        bool frozen = false;  ///< ... but plays another level: don't move (the switch is on its way)
        bool restart = false; ///< first frame of this level's segment: restart the level first
        PlayerInput input{};
    };

    /// Starts from the first frame (logs `replay: started ...`) and asks for level 1.
    void start();
    /// Stops a running replay without a verdict (SELECT; logs it).
    void stop();
    /// Hides the last verdict.
    void dismiss() { finished_ = false; }
    bool running() const { return running_; }
    /// The last replay ended and its verdict is on screen.
    bool finished() const { return finished_; }

    Drive drive(Demo level) const;
    /// The frame `drive(level)` asked for was played and left `sample`.
    void record(Demo level, const ReplaySample& sample);

    /// Bottom screen from `y` down: progress, or the verdict.
    void draw_panel(float y) const;
    /// Top screen: the verdict over the level (finished only).
    void draw_banner() const;

  private:
    void log_checkpoint(const ReplaySample& sample) const;
    void finish();

    ReplayRun run_;
    Demo playing_ = Demo::Level1; ///< level of the last recorded frame (for the checkpoint line)
    bool running_ = false;
    bool finished_ = false;
};
