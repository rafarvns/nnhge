// The queries example's self-check (examples/queries/self_check.hpp, spec 019 T7.3, AC-2): the script reaches the
// golden checksum on this host, twice the same, with a checkpoint every 1024 rays, and the lines
// examples/queries/tests/check.nnhs expects. Its checkpoints are pinned too, so a divergence shows where it starts.

#include "queries/self_check.hpp"
#include "test.hpp"

#include <cstdio>
#include <cstring>
#include <memory>

namespace {

// The checkpoint sums the simulator logs (pc-debug), as in examples/queries/tests/check.nnhs.
constexpr std::uint32_t kCheckpoints[4] = {0x36C6AD9Du, 0x6A48E776u, 0xF9B05D2Fu, 0xC8724E5Bu};

std::uint32_t run_to_end(std::uint32_t* checkpoints = nullptr, int* count = nullptr) {
    auto check = std::make_unique<QueriesSelfCheck>(); // the grid and boxes: too big for a small stack
    int n = 0;
    while (!check->done()) {
        if (!check->step()) continue;
        if (checkpoints != nullptr && n < 4) checkpoints[n] = check->checksum();
        ++n;
    }
    if (count != nullptr) *count = n;
    return check->checksum();
}

} // namespace

NNH_TEST(the_queries_self_check_reaches_the_golden_checksum) {
    std::uint32_t points[4] = {};
    int count = 0;
    const std::uint32_t sum = run_to_end(points, &count);
    if (sum != kQueriesCheckGolden) std::printf("  queries self-check sum %08X\n", static_cast<unsigned>(sum));
    for (int i = 0; i < 4; ++i) {
        if (points[i] != kCheckpoints[i]) std::printf("  checkpoint %d: %08X\n", (i + 1) * 1024, points[i]);
        CHECK(points[i] == kCheckpoints[i]);
    }
    CHECK(count == 4);
    CHECK(sum == kQueriesCheckGolden);
}

NNH_TEST(the_queries_self_check_gives_the_same_bits_twice) {
    CHECK(run_to_end() == run_to_end());
}

NNH_TEST(the_queries_self_check_runs_every_ray_and_cast_then_stops) {
    auto check = std::make_unique<QueriesSelfCheck>();
    while (!check->done())
        check->step();
    const std::uint32_t sum = check->checksum();
    CHECK(check->rays() == QueriesSelfCheck::kRays && check->casts() == QueriesSelfCheck::kCasts);
    CHECK(!check->step() && check->checksum() == sum);
}

NNH_TEST(queries_checkpoint_and_verdict_lines_have_the_scripts_format) {
    char line[96];
    format_queries_checkpoint(line, sizeof line, 1024, 0x0CF7E4CEu);
    CHECK(std::strcmp(line, "queries: 1024 sum 0CF7E4CE") == 0);
    format_queries_verdict(line, sizeof line, kQueriesCheckGolden);
    char expected[96];
    std::snprintf(expected, sizeof expected, "queries: PASS %08X", static_cast<unsigned>(kQueriesCheckGolden));
    CHECK(std::strcmp(line, expected) == 0);
    format_queries_verdict(line, sizeof line, kQueriesCheckGolden ^ 1u);
    CHECK(std::strncmp(line, "queries: FAIL sum ", 18) == 0);
}

NNH_TEST(the_queries_self_check_reaches_every_kind_of_result) {
    auto check = std::make_unique<QueriesSelfCheck>();
    while (!check->done())
        check->step();
    const QueriesSelfCheck::Stats& s = check->stats();
    std::printf("  grid %d, boxes %d, inside %d, one-way %d, blocked sights %d, casts %d\n", s.grid_hits, s.box_hits,
                s.inside_hits, s.one_way_hits, s.blocked_sights, s.cast_hits);
    CHECK(s.grid_hits > 1000 && s.box_hits > 500 && s.inside_hits > 50 && s.one_way_hits > 50);
    CHECK(s.blocked_sights > 1000 && s.blocked_sights < 4000 && s.cast_hits > 100);
}
