// The plural interpreter (spec 029 FR-17; plural.hpp): categories of integers under hand-assembled rules, and the
// validator's refusals. nnh-assetc's tests check the compiled CLDR rules against CLDR's samples.

#include <vector>

#include "loc/plural.hpp"
#include "loc_fixture.hpp"
#include "test.hpp"

using namespace loc_test;
namespace lf = nnh::detail::loc;

namespace {

lf::Category of(const std::vector<std::uint8_t>& rule, std::uint64_t n) {
    return lf::plural_category(rule.data(), rule.size(), n);
}

} // namespace

NNH_TEST(plural_english_is_one_for_one_only) {
    const auto en = Rules::english();
    CHECK(lf::plural_valid(en.data(), en.size()));
    CHECK(of(en, 1) == lf::One);
    CHECK(of(en, 0) == lf::Other);
    CHECK(of(en, 2) == lf::Other);
    CHECK(of(en, 11) == lf::Other);
    CHECK(of(en, 101) == lf::Other);
}

NNH_TEST(plural_russian_one_few_many) {
    const auto ru = Rules::russian();
    CHECK(lf::plural_valid(ru.data(), ru.size()));
    const std::uint64_t one[] = {1, 21, 31, 101, 1001, 1000001};
    for (std::uint64_t n : one) CHECK(of(ru, n) == lf::One);
    const std::uint64_t few[] = {2, 3, 4, 22, 24, 102, 1004};
    for (std::uint64_t n : few) CHECK(of(ru, n) == lf::Few);
    const std::uint64_t many[] = {0, 5, 9, 10, 11, 12, 14, 19, 20, 100, 111, 112, 1000};
    for (std::uint64_t n : many) CHECK(of(ru, n) == lf::Many);
}

NNH_TEST(plural_no_rule_is_always_other) {
    CHECK(lf::plural_category(nullptr, 0, 1) == lf::Other);
    CHECK(lf::plural_valid(nullptr, 0));
}

NNH_TEST(plural_validator_refuses_damaged_rules) {
    auto en = Rules::english();
    CHECK(!lf::plural_valid(en.data(), en.size() - 1)); // a range cut short
    auto bad_category = en;
    bad_category[1] = lf::Other; // only zero..many may be listed
    CHECK(!lf::plural_valid(bad_category.data(), bad_category.size()));
    auto bad_modulus = en;
    bad_modulus[4] = 5 << 1;
    CHECK(!lf::plural_valid(bad_modulus.data(), bad_modulus.size()));
    auto reversed = en;
    reversed[6] = 9; // lo 9 > hi 1
    CHECK(!lf::plural_valid(reversed.data(), reversed.size()));
    std::vector<std::uint8_t> order = {2, lf::Few, 1, 1, 0, 1};
    Rules::range(order, 2, 2);
    order.insert(order.end(), {lf::One, 1, 1, 0, 1});
    Rules::range(order, 1, 1);
    CHECK(!lf::plural_valid(order.data(), order.size())); // categories in increasing order
}
