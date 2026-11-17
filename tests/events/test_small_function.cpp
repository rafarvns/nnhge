// Fixed-size, non-allocating callable (nnh/detail/small_function.hpp).

#include "nnh/detail/small_function.hpp"
#include "test.hpp"

using nnh::detail::SmallFunction;

namespace {

int g_calls = 0;
void free_function(int v) { g_calls += v; }

struct Counter {
    int total = 0;
    void add(int v) { total += v; }
};

} // namespace

NNH_TEST(empty_function_is_false) {
    SmallFunction<void(int)> f;
    CHECK(!f);
}

NNH_TEST(stores_and_calls_a_free_function) {
    g_calls = 0;
    SmallFunction<void(int)> f = free_function;
    CHECK(static_cast<bool>(f));
    f(3);
    f(4);
    CHECK(g_calls == 7);
}

NNH_TEST(stores_a_lambda_with_small_captures) {
    int sum = 0;
    int* target = &sum;
    const int factor = 2;
    SmallFunction<void(int)> f = [target, factor](int v) { *target += v * factor; };
    f(5);
    CHECK(sum == 10);
}

NNH_TEST(returns_values) {
    SmallFunction<int(int, int)> add = [](int a, int b) { return a + b; };
    CHECK(add(2, 3) == 5);
}

NNH_TEST(copies_call_the_same_target) {
    Counter counter;
    Counter* c = &counter;
    SmallFunction<void(int)> f = [c](int v) { c->add(v); };
    SmallFunction<void(int)> g = f;
    f(1);
    g(2);
    CHECK(counter.total == 3);
}

NNH_TEST(capacity_fits_this_plus_a_pointer) {
    static_assert(nnh::detail::kSmallFunctionCapacity >= 2 * sizeof(void*));
    Counter counter;
    Counter* c = &counter;
    int* extra = &counter.total;
    SmallFunction<void(int)> f = [c, extra](int v) {
        c->add(v);
        *extra += 1;
    };
    f(10);
    CHECK(counter.total == 11);
}
