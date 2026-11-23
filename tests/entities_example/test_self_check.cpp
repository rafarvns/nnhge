// The entities example's self-check (examples/entities/self_check.hpp, spec 020 T5.4, AC-2): the script reaches the
// golden checksum on this host, twice the same, with a checkpoint every 10 000 operations, and the lines
// examples/entities/tests/check.nnhs expects. Its checkpoints are pinned too, so a divergence shows where it starts.

#include "entities/self_check.hpp"
#include "test.hpp"

#include <cstdio>
#include <cstring>
#include <memory>

namespace {

std::uint32_t run_to_end(std::uint32_t* checkpoints = nullptr, int* count = nullptr,
                         EntitiesSelfCheck::Stats* stats = nullptr) {
    auto check = std::make_unique<EntitiesSelfCheck>();
    CHECK(check->start());
    int n = 0;
    while (!check->done()) {
        if (!check->step()) continue;
        if (checkpoints != nullptr && n < 10) checkpoints[n] = check->checksum();
        ++n;
    }
    if (count != nullptr) *count = n;
    if (stats != nullptr) *stats = check->stats();
    return check->checksum();
}

} // namespace

NNH_TEST(the_entities_self_check_reaches_the_golden_checksum) {
    std::uint32_t points[10] = {};
    int count = 0;
    const std::uint32_t sum = run_to_end(points, &count);
    if (sum != kEntitiesCheckGolden) std::printf("  entities self-check sum %08X\n", static_cast<unsigned>(sum));
    for (int i = 0; i < 10; ++i) {
        if (points[i] != kEntitiesCheckpoints[i])
            std::printf("  checkpoint %d: %08X\n", (i + 1) * 10000, static_cast<unsigned>(points[i]));
        CHECK(points[i] == kEntitiesCheckpoints[i]);
    }
    CHECK(count == 10);
    CHECK(sum == kEntitiesCheckGolden);
}

NNH_TEST(the_entities_self_check_gives_the_same_bits_twice) { CHECK(run_to_end() == run_to_end()); }

NNH_TEST(the_entities_self_check_runs_every_operation_then_stops) {
    auto check = std::make_unique<EntitiesSelfCheck>();
    CHECK(check->start());
    while (!check->done()) check->step();
    const std::uint32_t sum = check->checksum();
    CHECK(check->operations() == EntitiesSelfCheck::kOperations);
    CHECK(!check->step() && check->checksum() == sum);
}

NNH_TEST(entities_checkpoint_and_verdict_lines_have_the_scripts_format) {
    char line[96];
    format_entities_checkpoint(line, sizeof line, 10000, 0x0CF7E4CEu);
    CHECK(std::strcmp(line, "entities: 10000 sum 0CF7E4CE") == 0);
    format_entities_verdict(line, sizeof line, kEntitiesCheckGolden);
    char expected[96];
    std::snprintf(expected, sizeof expected, "entities: PASS %08X", static_cast<unsigned>(kEntitiesCheckGolden));
    CHECK(std::strcmp(line, expected) == 0);
    format_entities_verdict(line, sizeof line, kEntitiesCheckGolden ^ 1u);
    CHECK(std::strncmp(line, "entities: FAIL sum ", 19) == 0);
}

NNH_TEST(the_entities_self_check_reaches_every_kind_of_operation) {
    EntitiesSelfCheck::Stats s;
    run_to_end(nullptr, nullptr, &s);
    std::printf("  spawns %d, destroys %d, hook destroys %d, hook spawns %d, clears %d, visits %d, finds %d, "
                "acquires %d, pool misses %d, max live %d\n",
                s.spawns, s.destroys, s.hook_destroys, s.hook_spawns, s.clears, s.visits, s.finds, s.acquires,
                s.pool_misses, s.max_live);
    CHECK(s.spawns > 5000 && s.destroys > 1000 && s.hook_destroys > 100 && s.hook_spawns > 50);
    CHECK(s.clears > 3 && s.visits > 100000 && s.finds > 1000 && s.acquires > 5000 && s.pool_misses > 0);
    CHECK(s.max_live > 200);
}
