// The world against the reference model (spec 020 T3.4, T4.3; FR-13..FR-15, FR-17, FR-19, FR-21): 50 seeded sequences
// of 5000 operations (spawn, destroy, add/remove of three kinds whose hooks destroy and spawn, flush, clear,
// iterations with destroys and spawns inside, tags, find), compared after every operation; the event bus untouched.

#include "model_driver.hpp"
#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

#include <cstdio>

namespace {

std::uint32_t next(std::uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

/// Runs `ops` seeded operations; returns the driver's sum, or 0 on a mismatch (printed).
std::uint32_t run(std::uint32_t seed, int ops, int tags) {
    auto* d = new model_test::Driver(tags);
    std::uint32_t s = seed * 2654435761u + 1;
    for (int i = 0; i < ops; ++i) {
        const std::uint32_t op = next(s);
        const std::uint32_t arg = next(s);
        if (!d->step(op, arg)) {
            std::printf("  seed %u op %d: %s\n", seed, i, d->mismatch());
            delete d;
            return 0;
        }
    }
    const std::uint32_t sum = d->sum();
    delete d;
    return sum;
}

} // namespace

NNH_TEST(world_equals_the_reference_model_on_50_seeded_sequences) {
    int failures = 0;
    for (std::uint32_t seed = 1; seed <= 50; ++seed)
        failures += run(seed, 5000, 1 + static_cast<int>(seed % 8)) == 0 ? 1 : 0;
    CHECK(failures == 0);
}

NNH_TEST(world_equals_the_reference_model_without_tags) { CHECK(run(77, 3000, 0) != 0); }

NNH_TEST(world_operations_publish_no_events) {
    // Without nnh::init() a publish asserts (events::publish before init): in Debug/Development a single engine
    // event from any operation, hook or clear of these 2000 would stop this test.
    nnh::shutdown();
    CHECK(run(5, 2000, 4) != 0);
}

NNH_TEST(world_model_runs_are_repeatable) { CHECK(run(9, 2000, 4) == run(9, 2000, 4)); }
