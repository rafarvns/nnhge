// The models example's self-check (spec 026 AC-2, T1.7/T4.10): the 50 000-operation script gives the golden checksum on
// every host compiler (and the console prints the same, AC-7); running it in slices of any size gives the same sums.

#include <cstdio>

#include "models/self_check.hpp"
#include "test.hpp"

NNH_TEST(models_self_check_matches_the_golden) {
    models::SelfCheck check;
    while (!check.run(997)) {
    }
    for (int k = 1; k <= 5; ++k) std::printf("    3d: %d sum %08X\n", k * 10000, check.checkpoint(k));
    CHECK(check.done() == models::SelfCheck::kOps);
    CHECK(check.sum() == models::kSelfCheckGolden);
}

NNH_TEST(models_self_check_is_independent_of_the_slice_size) {
    models::SelfCheck a, b;
    while (!a.run(10000)) {
    }
    while (!b.run(1)) {
    }
    CHECK(a.sum() == b.sum());
    for (int k = 1; k <= 5; ++k) CHECK(a.checkpoint(k) == b.checkpoint(k));
}
