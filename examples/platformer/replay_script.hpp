#pragma once
// The replay's script (spec 012 T7.3): level 1 for 600 frames, then the bonus level for 300, with inputs
// written down here (no buttons, no clock, no randomness), so every run plays the same frames.

#include "replay_run.hpp"

/// The replay's segments, in order; `count` receives how many.
const ReplaySegment* replay_segments(int& count);

/// "level1" / "bonus" (for log lines), or "?".
const char* level_name(Demo level);
