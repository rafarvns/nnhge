// log.txt (rotated at start) and crash.txt written through nnh::fs.

#include <cstring>

#include "crash/crash_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/assert.hpp"
#include "nnh/build.hpp"
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

// Each write to the 3DS SD card costs ~20 ms (a missed frame per line, found by the platformer's replay), so
// info/debug lines are batched; a warning writes them out at once, in order.
NNH_TEST(info_lines_wait_in_memory_until_a_warning_or_shutdown) {
    if constexpr (nnh::build::flavor() == nnh::build::Flavor::Release) return; // no info lines in Release
    nnh::shutdown();
    fake::reset();
    nnh::log::reset();
    CHECK(nnh::init());
    NNH_LOG_INFO(testfiles, "batched info line");
    CHECK(!file_contains("log.txt", "batched info line"));
    NNH_LOG_WARN(testfiles, "a warning after it");
    CHECK(file_contains("log.txt", "batched info line\n[") && file_contains("log.txt", "a warning after it"));
    NNH_LOG_INFO(testfiles, "the last info line");
    nnh::shutdown();
    nnh::log::reset();
    CHECK(nnh::init()); // files are read through nnh::fs, so look at the previous run's log
    CHECK(file_contains("log.prev.txt", "the last info line"));
    nnh::shutdown();
}

NNH_TEST(a_full_batch_of_info_lines_is_written_out) {
    if constexpr (nnh::build::flavor() == nnh::build::Flavor::Release) return; // no info lines in Release
    nnh::shutdown();
    fake::reset();
    nnh::log::reset();
    CHECK(nnh::init());
    for (int i = 0; i < 120; ++i) NNH_LOG_INFO(testfiles, "line %03d of a long run, padded to a typical length", i);
    CHECK(file_contains("log.txt", "line 000 of a long run")); // ~11 KB: past the batch, under the read buffer
    CHECK(!file_contains("log.txt", "line 119 of a long run")); // still in the batch
    nnh::shutdown();
    nnh::log::reset();
    CHECK(nnh::init());
    CHECK(file_contains("log.prev.txt", "line 119 of a long run"));
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
