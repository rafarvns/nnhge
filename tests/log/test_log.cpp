// nnh::log end to end: macros, filters, sinks, ring, platform console (fake platform).

#include <cstring>

#include "nnh/app.hpp"
#include "nnh/log.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::LogLevel;

NNH_LOG_CHANNEL(testlog);

namespace {

struct Captured {
    int count = 0;
    LogLevel last_level = LogLevel::Trace;
    char last[nnh::log::kMaxLineLength] = {};
};

void capture(LogLevel level, const char* line, void* user) {
    auto* c = static_cast<Captured*>(user);
    ++c->count;
    c->last_level = level;
    std::strncpy(c->last, line, sizeof c->last - 1);
}

void fresh() {
    nnh::shutdown();
    fake::reset();
    nnh::log::reset();
    NNH_LOG_CHANNEL_REF(testlog).min_level = LogLevel::Trace;
}

} // namespace

NNH_TEST(a_message_reaches_the_platform_console_sinks_and_ring) {
    fresh();
    Captured c;
    CHECK(nnh::log::add_sink(capture, &c));
    NNH_LOG_WARN(testlog, "hello %d", 7); // warn: compiled in every flavor
    CHECK(c.count == 1);
    CHECK(c.last_level == LogLevel::Warn);
    CHECK(std::strstr(c.last, "W testlog hello 7") != nullptr);
    CHECK(fake::state().console_lines == 1);
    CHECK(std::strstr(fake::state().last_console_line, "hello 7") != nullptr);
    CHECK(nnh::log::ring_size() == 1);
    CHECK(std::strstr(nnh::log::ring_line(0), "hello 7") != nullptr);
    nnh::log::remove_sink(capture, &c);
}

NNH_TEST(channel_level_filters_lower_levels) {
    fresh();
    NNH_LOG_CHANNEL_REF(testlog).min_level = LogLevel::Error;
    NNH_LOG_WARN(testlog, "hidden");
    NNH_LOG_ERROR(testlog, "shown");
    CHECK(fake::state().console_lines == 1);
    CHECK(std::strstr(fake::state().last_console_line, "shown") != nullptr);
}

NNH_TEST(global_level_filters_every_channel) {
    fresh();
    nnh::log::set_global_level(LogLevel::Error);
    NNH_LOG_WARN(testlog, "hidden");
    NNH_LOG_ERROR(testlog, "shown");
    CHECK(fake::state().console_lines == 1);
    nnh::log::set_global_level(LogLevel::Trace);
}

NNH_TEST(removed_sinks_stop_receiving) {
    fresh();
    Captured c;
    CHECK(nnh::log::add_sink(capture, &c));
    nnh::log::remove_sink(capture, &c);
    NNH_LOG_ERROR(testlog, "x");
    CHECK(c.count == 0);
}

NNH_TEST(sink_table_has_a_fixed_capacity) {
    fresh();
    Captured c[nnh::log::kMaxSinks + 1];
    int added = 0;
    for (Captured& one : c) added += nnh::log::add_sink(capture, &one) ? 1 : 0;
    CHECK(added == nnh::log::kMaxSinks);
    for (Captured& one : c) nnh::log::remove_sink(capture, &one);
}

NNH_TEST(compiled_minimum_follows_the_flavor) {
#if defined(NNH_FLAVOR_DEBUG)
    CHECK(nnh::log::kCompiledMinLevel == LogLevel::Trace);
#elif defined(NNH_FLAVOR_DEVELOPMENT)
    CHECK(nnh::log::kCompiledMinLevel == LogLevel::Debug);
#else
    CHECK(nnh::log::kCompiledMinLevel == LogLevel::Warn);
#endif
}

NNH_TEST(init_failure_is_logged_with_the_platform_reason) {
    fresh();
    fake::set_init_fails(true);
    CHECK(!nnh::init());
    CHECK(std::strstr(fake::state().last_console_line, "fake init failure") != nullptr);
}
