// nnh::time clock and frame delta, driven by the fake platform's manual clock.

#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/time.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::Duration;

namespace {

void fresh_engine_at(std::int64_t start_ns) {
    nnh::shutdown();
    fake::reset();
    fake::set_time_ns(start_ns);
    CHECK(nnh::init());
}

} // namespace

NNH_TEST(now_counts_from_init) {
    fresh_engine_at(5'000'000'000);
    CHECK(nnh::time::now().ns == 0);
    fake::advance_time(Duration::from_ms(250));
    CHECK(nnh::time::now().ms() == 250);
    nnh::shutdown();
}

NNH_TEST(delta_is_zero_on_the_first_frame) {
    fresh_engine_at(0);
    fake::advance_time(Duration::from_ms(40));
    CHECK(nnh::frame());
    CHECK(nnh::time::delta().ns == 0);
    nnh::shutdown();
}

NNH_TEST(delta_is_the_time_between_frame_starts) {
    fresh_engine_at(0);
    CHECK(nnh::frame());
    fake::advance_time(Duration::from_ms(16));
    CHECK(nnh::frame());
    CHECK(nnh::time::delta().ms() == 16);
    fake::advance_time(Duration::from_ms(33));
    CHECK(nnh::frame());
    CHECK(nnh::time::delta().ms() == 33);
    nnh::shutdown();
}

NNH_TEST(fixed_step_runs_logic_by_elapsed_frame_time) {
    fresh_engine_at(0);
    nnh::time::FixedStep step(60);
    CHECK(nnh::frame());
    int first = 0;
    while (step.next()) ++first;
    CHECK(first == 0); // first frame has no elapsed time
    fake::advance_time(step.step() * 2);
    CHECK(nnh::frame());
    int second = 0;
    while (step.next()) ++second;
    CHECK(second == 2);
    nnh::shutdown();
}
