// Profiler scopes (nnh/profile.hpp), timed with the fake platform's manual clock.

#include <cstring>

#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/profile.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::Duration;

namespace {

void fresh() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
}

void work(Duration d) {
    NNH_PROFILE_SCOPE("work");
    fake::advance_time(d);
}

} // namespace

NNH_TEST(scopes_report_time_and_calls_of_the_previous_frame) {
    if (!nnh::build::kDevToolsEnabled) return; // compiled out in Release
    fresh();
    CHECK(nnh::frame());
    work(Duration::from_ms(3));
    work(Duration::from_ms(2));
    CHECK(nnh::frame()); // frame boundary publishes the previous frame's numbers
    nnh::profile::ScopeStats top[4];
    const int n = nnh::profile::slowest(top, 4);
    CHECK(n == 1);
    CHECK(std::strcmp(top[0].name, "work") == 0);
    CHECK(top[0].calls == 2);
    CHECK(top[0].total.ms() == 5);
    nnh::shutdown();
}

NNH_TEST(slowest_scopes_come_first) {
    if (!nnh::build::kDevToolsEnabled) return;
    fresh();
    CHECK(nnh::frame());
    {
        NNH_PROFILE_SCOPE("fast");
        fake::advance_time(Duration::from_ms(1));
    }
    {
        NNH_PROFILE_SCOPE("slow");
        fake::advance_time(Duration::from_ms(9));
    }
    CHECK(nnh::frame());
    nnh::profile::ScopeStats top[4];
    CHECK(nnh::profile::slowest(top, 4) == 2);
    CHECK(std::strcmp(top[0].name, "slow") == 0);
    CHECK(std::strcmp(top[1].name, "fast") == 0);
    nnh::shutdown();
}

NNH_TEST(nested_scopes_are_counted_separately) {
    if (!nnh::build::kDevToolsEnabled) return;
    fresh();
    CHECK(nnh::frame());
    {
        NNH_PROFILE_SCOPE("outer");
        fake::advance_time(Duration::from_ms(1));
        work(Duration::from_ms(4));
    }
    CHECK(nnh::frame());
    nnh::profile::ScopeStats top[4];
    CHECK(nnh::profile::slowest(top, 4) == 2);
    CHECK(std::strcmp(top[0].name, "outer") == 0 && top[0].total.ms() == 5);
    CHECK(std::strcmp(top[1].name, "work") == 0 && top[1].total.ms() == 4);
    nnh::shutdown();
}
