// Integers as a language writes them (spec 029 FR-18, FR-24; loc_number.hpp): digits, the sign, the group separator
// from the language's minimum digit count, exact for INT64_MIN.

#include <climits>
#include <string>

#include "loc/loc_number.hpp"
#include "test.hpp"

namespace {

std::string number(long long v, const char* grouping, int min) {
    char out[nnh::detail::loc::kMaxNumberBytes];
    return std::string(out, static_cast<std::size_t>(nnh::detail::loc::format_integer(v, grouping, min, out)));
}

} // namespace

NNH_TEST(loc_number_groups_by_three_from_the_minimum_digits) {
    CHECK(number(0, ",", 4) == "0");
    CHECK(number(7, ",", 4) == "7");
    CHECK(number(999, ",", 4) == "999");
    CHECK(number(1000, ",", 4) == "1,000");
    CHECK(number(1000, ",", 5) == "1000");
    CHECK(number(12345, ",", 5) == "12,345");
    CHECK(number(1234567, ".", 4) == "1.234.567");
    CHECK(number(100000, ",", 1) == "100,000");
}

NNH_TEST(loc_number_negatives_and_the_extremes) {
    CHECK(number(-1, ",", 4) == "-1");
    CHECK(number(-1000, ",", 4) == "-1,000");
    CHECK(number(LLONG_MAX, ",", 4) == "9,223,372,036,854,775,807");
    CHECK(number(LLONG_MIN, ",", 4) == "-9,223,372,036,854,775,808");
    CHECK(number(LLONG_MIN, "\xE2\x80\xAF", 1).size() == 20 + 6 * 3);
}

NNH_TEST(loc_number_multi_byte_and_absent_separators) {
    CHECK(number(12345, "\xC2\xA0", 5) == "12\xC2\xA0" "345");
    CHECK(number(12345, "", 1) == "12345");
    CHECK(number(12345, nullptr, 1) == "12345");
}

NNH_TEST(loc_number_around_the_32_bit_boundary) {
    // T10.3: the digits below 2^32 come from a 32-bit loop, the ones above from the 64-bit one
    CHECK(number(4294967295LL, ",", 4) == "4,294,967,295");
    CHECK(number(4294967296LL, ",", 4) == "4,294,967,296");
    CHECK(number(-4294967297LL, "", 1) == "-4294967297");
    CHECK(number(10000000000LL, ",", 4) == "10,000,000,000");
    CHECK(number(42949672950LL, ".", 4) == "42.949.672.950");
}
