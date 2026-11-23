// The prefabs example's self-check (examples/prefabs/self_check.hpp, spec 021 T5.4, AC-2): prefab spawns and their
// hand-written equivalents agree after every one of 50 000 operations, the script reaches the golden checksum on this
// host, twice the same, with a checkpoint every 10 000 operations, and the lines examples/prefabs/tests/check.nnhs
// expects. The checkpoints are pinned too, so a divergence shows where it starts.

#include "prefabs/self_check.hpp"
#include "test.hpp"

#include <cstdio>
#include <cstring>
#include <memory>

namespace {

std::uint32_t run_to_end(std::uint32_t* checkpoints = nullptr, int* count = nullptr,
                         PrefabsSelfCheck::Stats* stats = nullptr, int* failed_step = nullptr) {
    auto check = std::make_unique<PrefabsSelfCheck>();
    CHECK(check->start());
    int n = 0;
    while (!check->done()) {
        if (!check->step()) continue;
        if (checkpoints != nullptr && n < 5) checkpoints[n] = check->checksum();
        ++n;
    }
    if (count != nullptr) *count = n;
    if (stats != nullptr) *stats = check->stats();
    if (failed_step != nullptr) *failed_step = check->failed() ? check->failed_step() : -1;
    return check->checksum();
}

} // namespace

NNH_TEST(prefab_spawns_equal_the_hand_written_calls_over_the_whole_script) {
    int failed = 0;
    run_to_end(nullptr, nullptr, nullptr, &failed);
    if (failed >= 0) std::printf("  the sides differ at step %d\n", failed);
    CHECK(failed == -1);
}

NNH_TEST(the_prefabs_self_check_reaches_the_golden_checksum) {
    std::uint32_t points[5] = {};
    int count = 0;
    const std::uint32_t sum = run_to_end(points, &count);
    if (sum != kPrefabsCheckGolden) std::printf("  prefabs self-check sum %08X\n", static_cast<unsigned>(sum));
    for (int i = 0; i < 5; ++i) {
        if (points[i] != kPrefabsCheckpoints[i])
            std::printf("  checkpoint %d: %08X\n", (i + 1) * 10000, static_cast<unsigned>(points[i]));
        CHECK(points[i] == kPrefabsCheckpoints[i]);
    }
    CHECK(count == 5);
    CHECK(sum == kPrefabsCheckGolden);
}

NNH_TEST(the_prefabs_self_check_gives_the_same_bits_twice) { CHECK(run_to_end() == run_to_end()); }

NNH_TEST(the_prefabs_self_check_runs_every_operation_then_stops) {
    auto check = std::make_unique<PrefabsSelfCheck>();
    CHECK(check->start());
    while (!check->done()) check->step();
    const std::uint32_t sum = check->checksum();
    CHECK(check->operations() == PrefabsSelfCheck::kOperations);
    CHECK(!check->step() && check->checksum() == sum);
}

NNH_TEST(prefabs_checkpoint_and_verdict_lines_have_the_scripts_format) {
    char line[96];
    format_prefabs_checkpoint(line, sizeof line, 10000, 0x0CF7E4CEu);
    CHECK(std::strcmp(line, "prefabs: 10000 sum 0CF7E4CE") == 0);
    auto check = std::make_unique<PrefabsSelfCheck>();
    CHECK(check->start());
    format_prefabs_verdict(line, sizeof line, *check);
    CHECK(std::strncmp(line, "prefabs: FAIL sum ", 18) == 0 || std::strncmp(line, "prefabs: PASS ", 14) == 0);
}

NNH_TEST(the_prefabs_self_check_reaches_every_kind_of_operation) {
    PrefabsSelfCheck::Stats s;
    run_to_end(nullptr, nullptr, &s);
    std::printf("  spawns %d, misses %d, hook spawns %d, suicides %d, D-hook spawns %d, overrides %d, positions %d, "
                "clears %d, visits %d, state checks %d, max live %d\n",
                s.spawns, s.misses, s.hook_spawns, s.suicides, s.d_spawns, s.overrides, s.positions, s.clears,
                s.visits, s.state_checks, s.max_live);
    CHECK(s.spawns > 10000 && s.misses > 100 && s.hook_spawns > 500 && s.suicides > 100 && s.d_spawns > 50);
    CHECK(s.overrides > 5000 && s.positions > 2000 && s.clears >= 3 && s.visits > 10000 && s.state_checks >= 50);
    CHECK(s.max_live > 300);
}
