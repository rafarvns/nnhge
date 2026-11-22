#include "replay_script.hpp"

namespace {

constexpr PlayerInput kIdle{};
constexpr PlayerInput kRight{.run = 1};
constexpr PlayerInput kLeft{.run = -1};
constexpr PlayerInput kJumpRight{.run = 1, .jump = true};
constexpr PlayerInput kDrop{.jump = true, .down = true};

// Level 1 (assets/levels/level1.tmj; `python -I art/make_level.py --show` prints it).
constexpr ReplayStep kLevel1[] = {
    {20, kIdle},      // land at the spawn point
    {40, kRight},     // the first coins
    {30, kJumpRight}, // up onto the first plank (one-way: from below), its coins
    {60, kRight},     // off its end, to the first pit
    {40, kJumpRight}, // over the pit, onto higher ground
    {110, kRight},    // under the bricks
    {40, kJumpRight}, // over the second pit
    {70, kRight},     //
    {40, kJumpRight}, // onto the bridge over the third pit
    {32, kRight},     // along it, past the pit
    {10, kIdle},      //
    {30, kDrop},      // Down + A: through the bridge to the ground below
    {26, kRight},     // to the stone steps
    {30, kJumpRight}, // up the steps
    {22, kRight},
};

// The bonus level (bonus_level.cpp): run, a jump into the bricks above (a ceiling), over the first pit.
constexpr ReplayStep kBonus[] = {
    {10, kIdle},      {40, kRight}, {30, kJumpRight}, {88, kRight},
    {40, kJumpRight}, {52, kRight}, {20, kLeft},      {20, kIdle},
};

template <typename T, int N> constexpr int size(const T (&)[N]) {
    return N;
}

constexpr ReplaySegment kSegments[] = {
    {.level = Demo::Level1, .steps = kLevel1, .step_count = size(kLevel1), .frames = 600},
    {.level = Demo::Bonus, .steps = kBonus, .step_count = size(kBonus), .frames = 300},
};

} // namespace

const ReplaySegment* replay_segments(int& count) {
    count = size(kSegments);
    return kSegments;
}

const char* level_name(Demo level) {
    switch (level) {
    case Demo::Level1:
        return "level1";
    case Demo::Bonus:
        return "bonus";
    default:
        return "?";
    }
}
