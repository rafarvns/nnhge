// The evaluation's exact results on a mixed workload (spec 017 console bench refinement): every public reading of
// 64 actions x 4 slots of every source kind, over 2000 frames with a second context toggled, repeats and thresholds
// on their edges, hashed bit for bit. The golden value was taken before the evaluation was optimised, so faster
// code must give exactly the same readings.

#include "actions_fixture.hpp"
#include "eval_workload.hpp"

using namespace actions_test;
namespace w = actions_workload;

namespace {

constexpr std::uint32_t kGolden = 0x4AE2673Fu;

std::uint32_t run_workload() {
    restart();
    nnh::ActionSet set{{.actions = w::kActions, .contexts = 2}};
    w::declare(set, true);
    w::Lcg rng;
    nnh::detail::InputView v = w::view_for(rng);
    std::uint32_t h = 0x811C9DC5u;
    for (int f = 0; f < 2000; ++f) {
        if (f % 3 == 0) v = w::view_for(rng);
        if (f % 37 == 0) set.enable(nnh::ContextId(1), (f / 37) % 2 == 0);
        nnh::detail::evaluate(*block_of(set), v);
        h = w::hash_readings(set, h);
    }
    return h;
}

} // namespace

NNH_TEST(a_mixed_workload_gives_the_golden_readings_bit_for_bit) {
    const std::uint32_t first = run_workload();
    CHECK(first == kGolden);
    CHECK(run_workload() == first);
}
