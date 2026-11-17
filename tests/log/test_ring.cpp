// Ring buffer of recent log lines (log/ring.hpp).

#include <cstdio>
#include <cstring>

#include "log/ring.hpp"
#include "test.hpp"

using nnh::detail::LogRing;

NNH_TEST(ring_keeps_lines_in_order_oldest_first) {
    LogRing ring;
    ring.push("one");
    ring.push("two");
    CHECK(ring.size() == 2);
    CHECK(std::strcmp(ring.line(0), "one") == 0);
    CHECK(std::strcmp(ring.line(1), "two") == 0);
}

NNH_TEST(ring_drops_the_oldest_lines_when_full) {
    LogRing ring;
    char text[16];
    for (int i = 0; i < nnh::log::kRingLines + 5; ++i) {
        std::snprintf(text, sizeof text, "line %d", i);
        ring.push(text);
    }
    CHECK(ring.size() == nnh::log::kRingLines);
    CHECK(std::strcmp(ring.line(0), "line 5") == 0);
    std::snprintf(text, sizeof text, "line %d", nnh::log::kRingLines + 4);
    CHECK(std::strcmp(ring.line(ring.size() - 1), text) == 0);
}

NNH_TEST(ring_can_be_cleared) {
    LogRing ring;
    ring.push("x");
    ring.clear();
    CHECK(ring.size() == 0);
}
