// The live example's self-check (spec 030 T7.1, AC-2): the defaults' golden and the check script's golden, the same on
// every host compiler (and on the console); one bit of one tunable changes the sum.

#include "live/self_check.hpp"
#include "test.hpp"

NNH_TEST(live_example_defaults_golden) { CHECK(live_self_check(Tuning{}) == kLiveDefaultsGolden); }

NNH_TEST(live_example_script_golden) { CHECK(live_self_check(live_script_tuning()) == kLiveScriptGolden); }

NNH_TEST(live_example_one_bit_changes_the_sum) {
    Tuning t;
    t.jump = 5.5000005f; // the next float above 5.5
    CHECK(live_self_check(t) != kLiveDefaultsGolden);
    Tuning c;
    c.sky.b = static_cast<std::uint8_t>(c.sky.b + 1);
    CHECK(live_self_check(c) != kLiveDefaultsGolden);
}
