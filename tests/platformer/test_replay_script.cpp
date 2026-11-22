// The platformer's replay script (examples/platformer/replay_script.hpp): level 1 for 600 frames, then the
// bonus level for 300 (the console script tests/level.nnhs counts on these numbers).

#include "platformer/replay_script.hpp"
#include "test.hpp"

#include <cstring>

NNH_TEST(replay_script_plays_level_1_then_the_bonus_level) {
    int count = 0;
    const ReplaySegment* segments = replay_segments(count);
    CHECK(count == 2);
    CHECK(segments[0].level == Demo::Level1);
    CHECK(segments[0].frames == 600);
    CHECK(segments[1].level == Demo::Bonus);
    CHECK(segments[1].frames == 300);
}

NNH_TEST(replay_script_inputs_fill_each_segment) {
    int count = 0;
    const ReplaySegment* segments = replay_segments(count);
    for (int s = 0; s < count; ++s) {
        int frames = 0;
        for (int i = 0; i < segments[s].step_count; ++i)
            frames += segments[s].steps[i].frames;
        CHECK(frames == segments[s].frames); // no idle tail, no step cut short
    }
}

NNH_TEST(replay_script_names_the_levels_like_the_log) {
    CHECK(std::strcmp(level_name(Demo::Level1), "level1") == 0);
    CHECK(std::strcmp(level_name(Demo::Bonus), "bonus") == 0);
    CHECK(std::strcmp(level_name(Demo::Bench), "?") == 0);
}
