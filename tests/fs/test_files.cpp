// nnh::fs file operations and atomic writes, on the fake platform's in-memory file system.

#include <cstring>

#include "nnh/app.hpp"
#include "nnh/fs.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
namespace fs = nnh::fs;

namespace {

void fresh() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
}

std::size_t read_text(const char* path, char* out, std::size_t cap) {
    auto r = fs::read_file(path, out, cap - 1);
    if (!r) return 0;
    out[r.value()] = '\0';
    return r.value();
}

} // namespace

NNH_TEST(written_files_can_be_read_back) {
    fresh();
    CHECK(fs::write_file("notes/a.txt", "hello", 5).ok());
    CHECK(fs::exists("notes/a.txt"));
    char text[16];
    CHECK(read_text("notes/a.txt", text, sizeof text) == 5);
    CHECK(std::strcmp(text, "hello") == 0);
    CHECK(fs::file_size("notes/a.txt").value_or(0) == 5);
    nnh::shutdown();
}

NNH_TEST(invalid_paths_never_reach_the_platform) {
    fresh();
    const int before = fake::state().fs_calls;
    CHECK(!fs::write_file("../escape.txt", "x", 1).ok());
    CHECK(!fs::read_file("/etc/passwd", nullptr, 0).ok());
    CHECK(fake::state().fs_calls == before);
    nnh::shutdown();
}

NNH_TEST(missing_files_report_not_found) {
    fresh();
    char buf[4];
    const auto r = fs::read_file("nope.bin", buf, sizeof buf);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::NotFound);
    CHECK(!fs::exists("nope.bin"));
    nnh::shutdown();
}

NNH_TEST(atomic_write_replaces_and_keeps_a_backup) {
    fresh();
    CHECK(fs::write_file_atomic("cfg.txt", "one", 3).ok());
    CHECK(fs::write_file_atomic("cfg.txt", "two", 3).ok());
    char text[8];
    read_text("cfg.txt", text, sizeof text);
    CHECK(std::strcmp(text, "two") == 0);
    read_text("cfg.txt.bak", text, sizeof text);
    CHECK(std::strcmp(text, "one") == 0);
    CHECK(!fs::exists("cfg.txt.tmp"));
    nnh::shutdown();
}

NNH_TEST(failed_temp_write_leaves_the_old_file_intact) {
    fresh();
    CHECK(fs::write_file_atomic("cfg.txt", "good", 4).ok());
    fake::fail_next_writes(1);
    CHECK(!fs::write_file_atomic("cfg.txt", "bad!", 4).ok());
    char text[8];
    read_text("cfg.txt", text, sizeof text);
    CHECK(std::strcmp(text, "good") == 0);
    nnh::shutdown();
}

NNH_TEST(buffer_too_small_is_an_error_not_an_overflow) {
    fresh();
    CHECK(fs::write_file("big.bin", "0123456789", 10).ok());
    char small[4] = {'x', 'x', 'x', 'x'};
    const auto r = fs::read_file("big.bin", small, 3);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::LimitReached);
    CHECK(small[3] == 'x');
    nnh::shutdown();
}

NNH_TEST(file_operations_need_init) {
    nnh::shutdown();
    fake::reset();
    CHECK(!fs::write_file("a.txt", "x", 1).ok());
}
