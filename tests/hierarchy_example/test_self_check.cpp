// The hierarchy example's self-check (spec 022 T5.5, AC-2): 50 000 operations on twin worlds — prefab hierarchies
// against hand-written calls — with the same handles every step, equal full states, world_now equal to every stored
// world after each pass, and the golden checksum and checkpoints on every host compiler. The script must reach what it
// claims (full kinds, cascades, hooks, refusals, walks, clears, deep trees), and a one-value mistake in the hand table
// must fail it.

#include "hierarchy/self_check.hpp"
#include "test.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

HierarchySelfCheck* run_all(HierarchySelfCheck& check, std::uint32_t* checkpoints) {
    CHECK(check.start());
    int k = 0;
    while (!check.done())
        if (check.step() && k < 5) checkpoints[k++] = check.checksum();
    return &check;
}

} // namespace

NNH_TEST(hierarchy_self_check_matches_its_golden) {
    static HierarchySelfCheck check; // two worlds and a library: off the stack
    std::uint32_t checkpoints[5] = {};
    run_all(check, checkpoints);
    if (std::getenv("NNH_GOLDEN_REPORT") != nullptr) {
        std::printf("hierarchy golden %08X checkpoints", static_cast<unsigned>(check.checksum()));
        for (std::uint32_t c : checkpoints) std::printf(" %08X", static_cast<unsigned>(c));
        const HierarchySelfCheck::Stats s = check.stats();
        std::printf("\nspawns %d misses %d hook_spawns %d d_hooks %d cascades %d attaches %d refusals %d detaches %d "
                    "removals %d walks %d clears %d passes %d world_now %d states %d max_live %d max_depth %d\n",
                    s.spawns, s.misses, s.hook_spawns, s.d_hooks, s.cascades, s.attaches, s.refusals, s.detaches,
                    s.removals, s.walks, s.clears, s.passes, s.world_now_checks, s.state_checks, s.max_live,
                    s.max_depth);
    }
    CHECK(!check.failed());
    CHECK(check.operations() == HierarchySelfCheck::kOperations);
    CHECK(check.checksum() == kHierarchyCheckGolden);
    for (int i = 0; i < 5; ++i) CHECK(checkpoints[i] == kHierarchyCheckpoints[i]);
    const HierarchySelfCheck::Stats s = check.stats();
    CHECK(s.spawns > 2000 && s.misses > 0 && s.hook_spawns > 0 && s.d_hooks > 0 && s.cascades > 0);
    CHECK(s.attaches > 1000 && s.refusals > 100 && s.detaches > 100 && s.removals > 100 && s.walks > 100);
    CHECK(s.clears > 0 && s.passes == HierarchySelfCheck::kOperations / HierarchySelfCheck::kPassEvery);
    CHECK(s.world_now_checks > 100000 && s.state_checks >= 50 && s.max_live > 500 && s.max_depth >= 6);
}
