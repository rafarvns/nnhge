// The platformer's doors (spec 023 AC-7): the door transition's options and the door run's count, lines and verdict.
// The whole run plays in the simulator (ctest platformer_doors_script_pc, examples/platformer/tests/doors.nnhs).

#include <cstring>

#include "nnh/log.hpp"
#include "platformer/door_run.hpp"
#include "platformer/door_transition.hpp"
#include "test.hpp"

NNH_LOG_CHANNEL(platformer);

namespace tr = nnh::transition;

NNH_TEST(the_door_transition_is_a_tall_iris_on_the_player) {
    const tr::Options o = door_transition({123, 45});
    CHECK(o.effect.kind == tr::Kind::Iris && o.effect.shape == tr::IrisShape::Circle && o.effect.band == 4);
    const nnh::Vec2 centre{123, 45};
    CHECK(o.effect.center.given && o.effect.center.point == centre && o.effect.center.screen == nnh::Screen::Top);
    CHECK(o.tall && o.block_input && o.reveal.kind == tr::Kind::Same);
    CHECK(o.out == nnh::FrameTime(nnh::Frames{36}) && o.hold == nnh::FrameTime(nnh::Frames{6}) &&
          o.in == nnh::FrameTime(nnh::Frames{36}));
    CHECK(o.out_ease.builtin == nnh::Ease::QuadIn && o.in_ease.builtin == nnh::Ease::QuadIn);
}

NNH_TEST(the_door_run_runs_right_and_counts_ten_doors) {
    DoorRun run;
    CHECK(!run.running());
    run.door(Demo::Level1, Demo::Bonus); // not running: ignored
    CHECK(run.doors() == 0);
    run.start();
    CHECK(run.running() && run.input().run == 1 && !run.input().jump);
    CHECK(std::strcmp(run.last_line(), "doors: started") == 0);
    for (int i = 0; i < DoorRun::kDoors; ++i) {
        for (int f = 0; f < 30; ++f) run.frame(16.7);
        run.door(i % 2 == 0 ? Demo::Level1 : Demo::Bonus, i % 2 == 0 ? Demo::Bonus : Demo::Level1);
        if (i == 0) CHECK(std::strcmp(run.last_line(), "doors: 1 level1 -> bonus frame 30") == 0);
        if (i == 1) CHECK(std::strcmp(run.last_line(), "doors: 2 bonus -> level1 frame 60") == 0);
        CHECK(run.running());
        run.door_done();
    }
    CHECK(!run.running() && run.doors() == 10 && run.frames() == 300);
    CHECK(std::strstr(run.last_line(), "doors: done 10 irises frames 300") != nullptr);
    CHECK(std::strstr(run.last_line(), ": 60 Hz PASS") != nullptr);
}

NNH_TEST(a_slow_frame_fails_the_door_runs_60_hz_verdict) {
    DoorRun run;
    run.start();
    run.frame(16.7);
    run.frame(40.0);
    for (int i = 0; i < DoorRun::kDoors; ++i) run.door_done();
    CHECK(std::strstr(run.last_line(), ": 60 Hz FAIL") != nullptr);
}
