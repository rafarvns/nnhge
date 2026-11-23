// Test runner. Usage: nnh_tests [filter]  — runs tests whose name contains `filter`.

#include <cstring>

#include "test.hpp"

namespace {

struct Entry {
    const char* name;
    nnh::test::TestFn fn;
};

constexpr int kMaxTests = 4096;
Entry g_tests[kMaxTests];
int g_count = 0;
int g_failures = 0;

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
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    for (int i = 0; i < g_count; ++i) {
        if (filter != nullptr && std::strstr(g_tests[i].name, filter) == nullptr) continue;
        const int before = g_failures;
        g_tests[i].fn();
        ++run;
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", g_tests[i].name);
    }
    std::printf("%d tests, %d failures\n", run, g_failures);
    return g_failures == 0 ? 0 : 1;
}
