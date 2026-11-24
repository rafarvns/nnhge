#pragma once
// The door run (spec 023 AC-7, AC-8): Y in a level (or PLATFORMER_DEMO=doors) starts each level 24 px left of its
// door and makes the player run right, so every level walks into its door: ten iris transitions, level 1 ↔ bonus.
// Lines: `doors: started`, `doors: <n> <from> -> <to> frame <f>` (f: frames since the start), then `doors: done 10
// irises frames <F> worst <W> ms avg <A> ms: 60 Hz PASS|FAIL` (a frame over 25 ms missed its slot). Pure (tested in
// tests/platformer/test_door_run.cpp): the levels and the demo switcher feed it.

#include "frame_meter.hpp"
#include "game_events.hpp"
#include "player.hpp"

#include <cstddef>
#include <cstdint>

class DoorRun {
public:
    static constexpr int kDoors = 10;

    void start();
    bool running() const { return running_; }
    /// What the level plays while running: run right.
    PlayerInput input() const;
    /// One frame of the run took `ms` (counted while running).
    void frame(double ms);
    /// A door transition started from `from` to `to`.
    void door(Demo from, Demo to);
    /// A door transition finished; after the tenth, the verdict.
    void door_done();

    int doors() const { return doors_; }
    int frames() const { return frames_; }
    const FrameMeter& meter() const { return meter_; }
    /// The last line logged (for the tests and the HUD).
    const char* last_line() const { return line_; }

private:
    void log();

    FrameMeter meter_;
    int doors_ = 0;
    int finished_ = 0;
    int frames_ = 0;
    bool running_ = false;
    char line_[128] = {};
};
