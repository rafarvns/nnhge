#include "test.hpp"

namespace {

struct Entry {
    const char* name;
    nnh::test::TestFn fn;
};

Entry g_tests[256];
int g_count = 0;
int g_failures = 0;

} // namespace

void nnh::test::register_test(const char* name, TestFn fn) { g_tests[g_count++] = {name, fn}; }

void nnh::test::fail(const char* file, int line, const char* expr) {
    std::printf("  FAIL %s:%d: %s\n", file, line, expr);
    ++g_failures;
}

int main() {
    for (int i = 0; i < g_count; ++i) {
        const int before = g_failures;
        g_tests[i].fn();
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", g_tests[i].name);
    }
    std::printf("%d tests, %d failures\n", g_count, g_failures);
    return g_failures == 0 ? 0 : 1;
}
