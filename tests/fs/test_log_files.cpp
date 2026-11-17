// log.txt (rotated at start) and crash.txt written through nnh::fs.

#include <cstring>

#include "crash/crash_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/assert.hpp"
#include "nnh/fs.hpp"
#include "nnh/log.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
namespace fs = nnh::fs;

NNH_LOG_CHANNEL(testfiles);

namespace {

bool file_contains(const char* path, const char* text) {
    static char buffer[16384];
    auto r = fs::read_file(path, buffer, sizeof buffer - 1);
    if (!r) return false;
    buffer[r.value()] = '\0';
    return std::strstr(buffer, text) != nullptr;
}

void crash_now() { NNH_VERIFY(false, "boom for the report"); }

} // namespace

NNH_TEST(logs_are_appended_to_log_txt) {
    nnh::shutdown();
    fake::reset();
    nnh::log::reset();
    CHECK(nnh::init());
    NNH_LOG_WARN(testfiles, "written to the file");
    CHECK(file_contains("log.txt", "written to the file"));
    nnh::shutdown();
}

NNH_TEST(previous_log_is_kept_as_log_prev_txt) {
    nnh::shutdown();
    fake::reset();
    nnh::log::reset();
    CHECK(nnh::init());
    NNH_LOG_WARN(testfiles, "first session");
    nnh::shutdown();
    nnh::log::reset();
    CHECK(nnh::init()); // the fake keeps files across init when not reset
    CHECK(file_contains("log.prev.txt", "first session"));
    CHECK(!file_contains("log.txt", "first session"));
    nnh::shutdown();
}

NNH_TEST(crash_writes_a_full_report_file) {
    nnh::shutdown();
    fake::reset();
    nnh::log::reset();
    CHECK(nnh::init());
    NNH_LOG_WARN(testfiles, "context line");
    CHECK(fake::run_expecting_crash(crash_now));
    CHECK(file_contains("crash.txt", "boom for the report"));
    CHECK(file_contains("crash.txt", "context line"));
    CHECK(std::strstr(fake::last_crash().bottom, "crash.txt") != nullptr);
    nnh::detail::crash_install();
    nnh::shutdown();
}
