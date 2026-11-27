// Test runner. Usage: nnh_tests [--reverse] [filter...] — runs the tests whose name contains any filter (all without
// one), in registration order or, with --reverse, the other way round (another platform's link order may differ: a test
// that depends on what an earlier one left behind shows up here).

#include <cstring>

#include "test.hpp"

namespace {

struct Entry {
    const char* name;
    nnh::test::TestFn fn;
};

constexpr int kMaxTests = 8192;
Entry g_tests[kMaxTests];
int g_count = 0;
int g_failures = 0;

bool selected(const char* name, int filters, char** filter) {
    if (filters == 0) return true;
    for (int f = 0; f < filters; ++f) {
        if (std::strstr(name, filter[f]) != nullptr) return true;
    }
    return false;
}

} // namespace

void nnh::test::register_test(const char* name, TestFn fn) {
    if (g_count == kMaxTests) {
        std::printf("too many tests, raise kMaxTests in %s\n", __FILE__);
        return;
    }
    g_tests[g_count++] = {name, fn};
}

void nnh::test::fail(const char* file, int line, const char* expr) {
    std::printf("  FAIL %s:%d: %s\n", file, line, expr);
    ++g_failures;
}

int main(int argc, char** argv) {
    int first = 1;
    const bool reverse = argc > 1 && std::strcmp(argv[1], "--reverse") == 0;
    if (reverse) first = 2;
    int run = 0;
    for (int k = 0; k < g_count; ++k) {
        const int i = reverse ? g_count - 1 - k : k;
        if (!selected(g_tests[i].name, argc - first, argv + first)) continue;
        const int before = g_failures;
        g_tests[i].fn();
        ++run;
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", g_tests[i].name);
    }
    std::printf("%d tests, %d failures\n", run, g_failures);
    return g_failures == 0 ? 0 : 1;
}
