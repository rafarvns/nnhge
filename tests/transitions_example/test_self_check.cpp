// The transitions example's self-check (spec 023 T5.7, AC-3): its script on the fake platform must give the golden
// checkpoints recorded from pc-debug, on every host compiler (and the console must log the same).

#include <cstdio>

#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/transition.hpp"
#include "test.hpp"
#include "transitions/check_script.hpp"

namespace {

TransitionsCheck& check_object() {
    static TransitionsCheck check;
    return check;
}

/// Plays the whole script; returns the frames it took.
int play() {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({.gfx = {.max_draws = 2048}}));
    TransitionsCheck& check = check_object();
    // The object is static: an earlier play() left it done(), and the driver's on_enter (which restarts it) runs only
    // inside the first frame, after the loop's first done() test. Start it here, as a fresh object would be.
    check.reset();
    nnh::scenes::push(check.driver());
    int frames = 0;
    while (!check.done() && frames < 2000) {
        CHECK(nnh::frame());
        nnh::scenes::update_and_draw();
        ++frames;
    }
    nnh::shutdown();
    return frames;
}

} // namespace

NNH_TEST(transitions_self_check_matches_its_golden_checkpoints) {
    CHECK(play() == TransitionsCheck::kFrames);
    const TransitionsCheck& check = check_object();
    bool same = true;
    for (int i = 0; i < TransitionsCheck::kFrames / TransitionsCheck::kCheckpointEvery; ++i) {
        if (check.checkpoints()[i] != kTransitionsCheckpoints[i]) {
            same = false;
            std::printf("  transitions: %d sum %08X (golden %08X)\n", (i + 1) * TransitionsCheck::kCheckpointEvery,
                        static_cast<unsigned>(check.checkpoints()[i]), static_cast<unsigned>(kTransitionsCheckpoints[i]));
        }
    }
    CHECK(same);
    CHECK(check.checksum() == kTransitionsCheckGolden);
    char verdict[96];
    format_transitions_verdict(verdict, sizeof verdict, check);
    CHECK(same == (verdict[13] == 'P'));
}

NNH_TEST(transitions_self_check_plays_the_same_twice) {
    CHECK(play() == TransitionsCheck::kFrames);
    const std::uint32_t first = check_object().checksum();
    CHECK(play() == TransitionsCheck::kFrames); // the whole script again, not a finished one
    CHECK(check_object().checksum() == first);
}
