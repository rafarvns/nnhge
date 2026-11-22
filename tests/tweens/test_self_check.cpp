// The tweens example's self-check (examples/tweens/self_check.hpp, spec 015 T6.1, AC-5): the script reaches
// the golden checksum on this host, the same run twice gives the same bits, checkpoints come every 60 frames,
// and the verdict lines are what examples/tweens/tests/check.nnhs expects.

#include "test.hpp"
#include "tweens/self_check.hpp"

#include <cstdio>
#include <cstring>

namespace {

std::uint32_t run_to_end(int* checkpoints = nullptr) {
    SelfCheck check;
    int count = 0;
    while (!check.done()) {
        if (check.update()) ++count;
    }
    if (checkpoints != nullptr) *checkpoints = count;
    return check.checksum();
}

} // namespace

NNH_TEST(the_self_check_reaches_the_golden_checksum) {
    const std::uint32_t sum = run_to_end();
    if (sum != kSelfCheckGolden) std::printf("  self-check sum %08X\n", static_cast<unsigned>(sum));
    CHECK(sum == kSelfCheckGolden);
}

NNH_TEST(the_self_check_gives_the_same_bits_twice) {
    CHECK(run_to_end() == run_to_end());
}

NNH_TEST(the_self_check_has_a_checkpoint_every_60_frames) {
    int checkpoints = 0;
    run_to_end(&checkpoints);
    CHECK(checkpoints == SelfCheck::kFrames / SelfCheck::kCheckpointEvery);
    SelfCheck check;
    for (int i = 1; i < 60; ++i) {
        CHECK(!check.update());
    }
    CHECK(check.update() && check.frame() == 60);
}

NNH_TEST(the_self_check_stops_after_its_last_frame) {
    SelfCheck check;
    while (!check.done()) {
        check.update();
    }
    const std::uint32_t sum = check.checksum();
    CHECK(!check.update());
    CHECK(check.checksum() == sum && check.frame() == SelfCheck::kFrames);
}

NNH_TEST(the_checksum_changes_every_frame) {
    SelfCheck check;
    std::uint32_t last = check.checksum();
    for (int i = 0; i < 100; ++i) {
        check.update();
        CHECK(check.checksum() != last);
        last = check.checksum();
    }
}

NNH_TEST(this_host_rounds_to_nearest) {
    CHECK(rounding_probe_ok());
}

NNH_TEST(checkpoint_and_verdict_lines_have_the_scripts_format) {
    char line[96];
    format_checkpoint(line, sizeof line, 120, 0x0CF7E4CEu);
    CHECK(std::strcmp(line, "tweens: 120 sum 0CF7E4CE") == 0);
    format_verdict(line, sizeof line, kSelfCheckGolden, true);
    char expected[96];
    std::snprintf(expected, sizeof expected, "tweens: PASS %08X", static_cast<unsigned>(kSelfCheckGolden));
    CHECK(std::strcmp(line, expected) == 0);
    format_verdict(line, sizeof line, 0x12345678u, true);
    std::snprintf(expected, sizeof expected, "tweens: FAIL sum 12345678 expected %08X rounding ok",
                  static_cast<unsigned>(kSelfCheckGolden));
    CHECK(std::strcmp(line, expected) == 0 || kSelfCheckGolden == 0x12345678u);
    format_verdict(line, sizeof line, kSelfCheckGolden, false);
    CHECK(std::strncmp(line, "tweens: FAIL", 12) == 0 && std::strstr(line, "rounding WRONG") != nullptr);
}
