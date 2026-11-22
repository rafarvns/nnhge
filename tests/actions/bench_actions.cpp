// PC microbenchmark of one action set's evaluation (spec 017 console bench refinement): the actions example's bench
// set (64 actions x 4 slots) evaluated many times, for relative gains only (the console is the real measure).
// Build: cmake --build --preset pc-release --target nnh_actions_bench; run: build/pc-release/tests/nnh_actions_bench

#include "eval_workload.hpp"
#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"

#include <chrono>
#include <cstdio>

namespace w = actions_workload;

namespace {

constexpr int kViews = 256;
constexpr int kRounds = 20000;

nnh::detail::ActionBlock* block_of(nnh::ActionSet& set) {
    nnh::detail::ActionBlock* block = nullptr;
    std::memcpy(&block, &set, sizeof block);
    return block;
}

/// Nanoseconds per evaluation of the set over `views`, the best of 5 runs.
double measure(nnh::ActionSet& set, const nnh::detail::InputView* views, std::uint32_t& sink) {
    double best = 1e30;
    for (int run = 0; run < 5; ++run) {
        const auto start = std::chrono::steady_clock::now();
        for (int r = 0; r < kRounds; ++r) {
            nnh::detail::evaluate(*block_of(set), views[r % kViews]);
            sink += static_cast<std::uint32_t>(set.held(nnh::ActionId(r & 63)));
        }
        const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
        if (ns / kRounds < best) best = ns / kRounds;
    }
    return best;
}

} // namespace

int main() {
    nnh::platform_fake::reset();
    if (!nnh::init({})) return 1;
    static nnh::detail::InputView idle[kViews], mixed[kViews];
    w::Lcg rng;
    for (int i = 0; i < kViews; ++i) {
        idle[i] = nnh::detail::make_input_view(0, {}, {}, false, {});
        mixed[i] = w::view_for(rng);
    }
    std::uint32_t sink = 0;
    {
        nnh::ActionSet set{{.actions = w::kActions}};
        w::declare(set, false);
        const double idle_ns = measure(set, idle, sink);
        const double mixed_ns = measure(set, mixed, sink);
        std::printf("actions bench (64 x 4): idle %.1f ns/eval, mixed %.1f ns/eval (sink %u)\n", idle_ns, mixed_ns,
                    sink);
    }
    nnh::shutdown();
    return 0;
}
