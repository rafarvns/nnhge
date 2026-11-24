// The pathfinding example's self-check (spec 025 T6.2, AC-3): its golden checksum and checkpoints on every host
// compiler, the same in slices of 8 searches as in one run.

#include <cstdio>
#include <memory>

#include "pathfinding/self_check.hpp"
#include "test.hpp"

namespace pd = pathfinding_demo;

NNH_TEST(pathfinding_self_check_matches_its_golden_and_checkpoints) {
    std::uint32_t checkpoints[pd::kCheckpoints] = {};
    const std::uint32_t sum = pd::run_self_check(checkpoints);
    std::printf("  pathfinding self-check %08X; checkpoints", static_cast<unsigned>(sum));
    for (std::uint32_t c : checkpoints) std::printf(" %08X", static_cast<unsigned>(c));
    std::printf("\n");
    CHECK(sum == pd::kPathfindingCheckGolden);
    for (int k = 0; k < pd::kCheckpoints; ++k) CHECK(checkpoints[k] == pd::kPathfindingCheckpoints[k]);
}

NNH_TEST(pathfinding_self_check_in_slices_of_8_equals_one_run) {
    auto check = std::make_unique<pd::SelfCheck>();
    CHECK(check->init());
    int frames = 0;
    while (!check->done()) {
        CHECK(check->run(8) == 8);
        ++frames;
    }
    CHECK(frames == pd::kCheckSearches / 8);
    CHECK(check->checksum() == pd::run_self_check());
}
