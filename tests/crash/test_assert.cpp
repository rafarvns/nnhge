// NNH_ASSERT / NNH_VERIFY / NNH_PANIC reach the crash screen (fake platform intercepts it).

#include <cstring>

#include "crash/crash_internal.hpp"
#include "nnh/assert.hpp"
#include "nnh/build.hpp"
#include "nnh/log.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;

NNH_LOG_CHANNEL(testcrash);

namespace {

int g_side_effects = 0;
bool touch() {
    ++g_side_effects;
    return true;
}

void failing_verify() { NNH_VERIFY(1 + 1 == 3, "math is broken: %d", 42); }
void failing_assert() { NNH_ASSERT(false, "assert fired"); }
void passing_checks() {
    NNH_VERIFY(true);
    NNH_ASSERT(true, "never shown");
}
void panicking() { NNH_PANIC("unreachable state %s", "S7"); }

// Debug/Development show every detail on screen; Release shows a friendly message and the location.
constexpr bool kDetails = nnh::build::kDevToolsEnabled;

} // namespace

NNH_TEST(failed_verify_shows_crash_screen_with_location) {
    fake::reset();
    nnh::detail::crash_install();
    CHECK(fake::run_expecting_crash(failing_verify));
    const fake::Crash& crash = fake::last_crash();
    CHECK(std::strstr(crash.top, "test_assert.cpp:") != nullptr);
    if (kDetails) {
        CHECK(std::strstr(crash.top, "math is broken: 42") != nullptr);
        CHECK(std::strstr(crash.top, "1 + 1 == 3") != nullptr);
    }
}

NNH_TEST(passing_checks_do_not_crash) {
    fake::reset();
    nnh::detail::crash_install();
    CHECK(!fake::run_expecting_crash(passing_checks));
}

NNH_TEST(panic_always_crashes) {
    fake::reset();
    nnh::detail::crash_install();
    CHECK(fake::run_expecting_crash(panicking));
    if (kDetails) CHECK(std::strstr(fake::last_crash().top, "unreachable state S7") != nullptr);
}

NNH_TEST(assert_follows_the_flavor) {
    fake::reset();
    nnh::detail::crash_install();
    CHECK(fake::run_expecting_crash(failing_assert) == nnh::build::kAssertsEnabled);
}

NNH_TEST(disabled_assert_does_not_evaluate_its_condition) {
    g_side_effects = 0;
    NNH_ASSERT(touch(), "side effect");
    CHECK(g_side_effects == (nnh::build::kAssertsEnabled ? 1 : 0));
}

NNH_TEST(crash_is_logged_as_fatal_with_the_message) {
    fake::reset();
    nnh::detail::crash_install();
    nnh::log::reset();
    CHECK(fake::run_expecting_crash(failing_verify));
    CHECK(std::strstr(fake::state().last_console_line, "] F ") != nullptr);
    CHECK(std::strstr(fake::state().last_console_line, "math is broken: 42") != nullptr);
}

NNH_TEST(crash_screen_shows_recent_logs_in_detailed_flavors) {
    fake::reset();
    nnh::detail::crash_install();
    nnh::log::reset();
    NNH_LOG_WARN(testcrash, "just before the crash");
    CHECK(fake::run_expecting_crash(failing_verify));
    const bool shown = std::strstr(fake::last_crash().bottom, "just before the crash") != nullptr;
    CHECK(shown == kDetails);
}

namespace {

void faulting() {
    nnh::platform::CpuFault fault;
    fault.kind = "Data abort";
    fault.has_registers = true;
    for (int r = 0; r < 16; ++r) fault.regs[r] = 0x2000u + static_cast<unsigned>(r);
    fake::simulate_cpu_fault(fault);
}

} // namespace

NNH_TEST(cpu_fault_from_the_platform_shows_the_crash_screen) {
    fake::reset();
    nnh::detail::crash_install();
    CHECK(fake::run_expecting_crash(faulting));
    if (kDetails) {
        CHECK(std::strstr(fake::last_crash().top, "Data abort") != nullptr);
        CHECK(std::strstr(fake::last_crash().top, "pc 0000200F") != nullptr);
    }
}
