// nnh::Duration arithmetic and conversions.

#include "nnh/time.hpp"
#include "test.hpp"

using nnh::Duration;

NNH_TEST(duration_converts_between_units) {
    const Duration d = Duration::from_ms(1500);
    CHECK(d.ns == 1'500'000'000);
    CHECK(d.ms() == 1500);
    CHECK(d.seconds() > 1.499 && d.seconds() < 1.501);
    CHECK(Duration::from_us(3).ns == 3000);
    CHECK(Duration::from_seconds(2).ms() == 2000);
}

NNH_TEST(duration_supports_arithmetic_and_comparison) {
    const Duration a = Duration::from_ms(10);
    const Duration b = Duration::from_ms(4);
    CHECK((a + b).ms() == 14);
    CHECK((a - b).ms() == 6);
    CHECK(a > b && b < a && a != b);
    CHECK((b * 3).ms() == 12);
    Duration c = a;
    c += b;
    c -= Duration::from_ms(2);
    CHECK(c.ms() == 12);
}
