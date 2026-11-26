// Numbers (spec 028 T1.5, FR-25): signed decimals without printf, prefixes and suffixes, truncation at capacity.

#include <climits>
#include <cstring>

#include "test.hpp"
#include "ui/ui_number.hpp"

using nnh::detail::ui_format_number;

namespace {
bool is(const char* buffer, int length, const char* expected) {
    return length == static_cast<int>(std::strlen(expected)) && std::memcmp(buffer, expected, length) == 0;
}
} // namespace

NNH_TEST(ui_number_zero_and_one) {
    char b[16];
    CHECK(is(b, ui_format_number(b, 16, 0), "0"));
    CHECK(is(b, ui_format_number(b, 16, 1), "1"));
    CHECK(is(b, ui_format_number(b, 16, -1), "-1"));
    CHECK(is(b, ui_format_number(b, 16, 1234567), "1234567"));
}

NNH_TEST(ui_number_int_min_and_max) {
    char b[16];
    CHECK(is(b, ui_format_number(b, 16, INT_MAX), "2147483647"));
    CHECK(is(b, ui_format_number(b, 16, INT_MIN), "-2147483648"));
}

NNH_TEST(ui_number_prefix_and_suffix) {
    char b[16];
    CHECK(is(b, ui_format_number(b, 16, 12, "x"), "x12"));
    CHECK(is(b, ui_format_number(b, 16, 70, "", " %"), "70 %"));
    CHECK(is(b, ui_format_number(b, 16, -5, "(", ")"), "(-5)"));
}

NNH_TEST(ui_number_truncates_at_capacity) {
    char b[16];
    for (int cap = 0; cap <= 12; ++cap) {
        std::memset(b, '#', sizeof b);
        const int n = ui_format_number(b, cap, INT_MIN);
        CHECK(n == (cap < 11 ? cap : 11));
        CHECK(std::memcmp(b, "-2147483648", static_cast<std::size_t>(n)) == 0);
        CHECK(b[n] == '#'); // nothing written past the length
    }
    CHECK(is(b, ui_format_number(b, 3, 7, "abc"), "abc"));
    CHECK(is(b, ui_format_number(b, 4, 75, "x", "%"), "x75%"));
    CHECK(is(b, ui_format_number(b, 3, 75, "x", "%"), "x75"));
}
