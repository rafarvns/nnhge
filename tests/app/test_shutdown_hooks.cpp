// Shutdown hooks (spec 014 T3.3): optional modules (data assets) free what they keep at nnh::shutdown(),
// before the leak report, without the core knowing them.

#include <cstring>

#include "core/shutdown_hooks.hpp"
#include "nnh/app.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;

namespace {

NNH_MEM_TAG(hook_test);

int g_calls = 0;
int g_order[4] = {};
void* g_block = nullptr;

void first_hook() {
    g_order[g_calls++] = 1;
}
void second_hook() {
    g_order[g_calls++] = 2;
}
void free_block() {
    ++g_calls;
    nnh::mem::free(g_block);
    g_block = nullptr;
}
void counting_hook() {
    ++g_calls;
}

void fresh_engine() {
    nnh::shutdown();
    fake::reset();
    g_calls = 0;
    CHECK(nnh::init());
}

} // namespace

NNH_TEST(shutdown_runs_each_hook_once_newest_first) {
    fresh_engine();
    CHECK(nnh::detail::add_shutdown_hook(first_hook));
    CHECK(nnh::detail::add_shutdown_hook(second_hook));
    nnh::shutdown();
    CHECK(g_calls == 2);
    CHECK(g_order[0] == 2 && g_order[1] == 1);
}

NNH_TEST(shutdown_hooks_are_forgotten_after_they_run) {
    fresh_engine();
    CHECK(nnh::detail::add_shutdown_hook(counting_hook));
    nnh::shutdown();
    CHECK(nnh::init());
    nnh::shutdown();
    CHECK(g_calls == 1);
}

NNH_TEST(adding_the_same_shutdown_hook_twice_keeps_one) {
    fresh_engine();
    CHECK(nnh::detail::add_shutdown_hook(counting_hook));
    CHECK(nnh::detail::add_shutdown_hook(counting_hook));
    nnh::shutdown();
    CHECK(g_calls == 1);
}

NNH_TEST(shutdown_hooks_have_a_fixed_capacity) {
    fresh_engine();
    using Hook = void (*)();
    static void (*const kHooks[])() = {
        [] {}, [] {}, [] {}, [] {}, [] {}, [] {}, [] {}, [] {},
    };
    static_assert(sizeof kHooks / sizeof kHooks[0] == nnh::detail::kMaxShutdownHooks);
    for (Hook h : kHooks)
        CHECK(nnh::detail::add_shutdown_hook(h));
    CHECK(!nnh::detail::add_shutdown_hook(counting_hook));
    nnh::shutdown();
    CHECK(g_calls == 0);
}

NNH_TEST(shutdown_hooks_run_before_the_leak_report) {
    fresh_engine();
    g_block = nnh::mem::alloc(nnh::MemRegion::Heap, NNH_MEM_TAG_REF(hook_test), 64);
    CHECK(g_block != nullptr);
    CHECK(nnh::detail::add_shutdown_hook(free_block));
    nnh::shutdown();
    CHECK(g_calls == 1 && g_block == nullptr);
    CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(hook_test)).count == 0);
    CHECK(std::strstr(fake::state().last_console_line, "hook_test") == nullptr); // no leak line
}
