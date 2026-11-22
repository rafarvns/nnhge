// The animation example's self-check (examples/animation/self_check.hpp, spec 016 T8.2, AC-6): the script reaches the
// golden checksum on this host, the same run twice gives the same bits, checkpoints come every 60 frames, every kind of
// notice happens, and the verdict lines are what examples/animation/tests/check.nnhs expects.

#include "animation/self_check.hpp"
#include "test.hpp"

#include <cstdio>
#include <cstring>
#include <memory>

namespace {

std::uint32_t run_to_end(int* checkpoints = nullptr, int* notices = nullptr) {
    auto check = std::make_unique<AnimationSelfCheck>();
    int count = 0;
    while (!check->done()) {
        if (check->update()) ++count;
    }
    if (checkpoints != nullptr) *checkpoints = count;
    if (notices != nullptr) *notices = check->notices();
    return check->checksum();
}

} // namespace

NNH_TEST(the_animation_self_check_reaches_the_golden_checksum) {
    const std::uint32_t sum = run_to_end();
    if (sum != kAnimationSelfCheckGolden) std::printf("  animation self-check sum %08X\n", static_cast<unsigned>(sum));
    CHECK(sum == kAnimationSelfCheckGolden);
}

NNH_TEST(the_animation_self_check_gives_the_same_bits_twice_with_checkpoints) {
    int checkpoints = 0, notices = 0;
    CHECK(run_to_end(&checkpoints, &notices) == run_to_end());
    CHECK(checkpoints == AnimationSelfCheck::kFrames / AnimationSelfCheck::kCheckpointEvery);
    CHECK(notices > 2000);
}

NNH_TEST(the_animation_self_check_stops_after_its_last_frame) {
    auto check = std::make_unique<AnimationSelfCheck>();
    while (!check->done()) check->update();
    const std::uint32_t sum = check->checksum();
    CHECK(!check->update() && check->checksum() == sum && check->frame() == AnimationSelfCheck::kFrames);
}

NNH_TEST(animation_checkpoint_and_verdict_lines_have_the_scripts_format) {
    char line[96];
    format_animation_checkpoint(line, sizeof line, 120, 0x0CF7E4CEu);
    CHECK(std::strcmp(line, "animation: 120 sum 0CF7E4CE") == 0);
    format_animation_verdict(line, sizeof line, 600, kAnimationSelfCheckGolden);
    CHECK(std::strncmp(line, "animation: PASS ", 16) == 0);
    format_animation_verdict(line, sizeof line, 600, kAnimationSelfCheckGolden ^ 1u);
    char expected[96];
    std::snprintf(expected, sizeof expected, "animation: FAIL 600 %08X %08X",
                  static_cast<unsigned>(kAnimationSelfCheckGolden ^ 1u), static_cast<unsigned>(kAnimationSelfCheckGolden));
    CHECK(std::strcmp(line, expected) == 0);
}
