// The fake platform's power cuts and truncation (spec 027 T1.4, AC-3): the cut call keeps half a write when torn,
// a cut rename or remove does nothing, later mutating calls fail without effect, reads keep working, and
// restore_power() / reset() end the cut. The fs call log names each contract call.

#include <cstdint>
#include <cstring>
#include <string_view>

#include "nnh/platform/fs.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
namespace platform = nnh::platform;

namespace {

bool write(const char* path, const char* text) {
    const platform::FsChunk chunk{text, std::strlen(text)};
    return platform::fs_write(path, &chunk, 1);
}

std::int64_t size(const char* path) { return platform::fs_size(path); }

} // namespace

NNH_TEST(fake_power_cut_after_zero_cuts_the_next_write_in_half) {
    fake::reset();
    fake::cut_power_after(0);
    CHECK(!write("a.bin", "12345678"));
    CHECK(size("a.bin") == 4);
    fake::reset();
}

NNH_TEST(fake_power_cut_without_tearing_writes_nothing) {
    fake::reset();
    fake::cut_power_after(0, false);
    CHECK(!write("a.bin", "12345678"));
    CHECK(size("a.bin") == -1);
    fake::reset();
}

NNH_TEST(fake_power_cut_lets_the_first_operations_run) {
    fake::reset();
    CHECK(write("old.bin", "old"));
    fake::cut_power_after(2);
    CHECK(write("a.bin", "1234"));                      // 1
    CHECK(platform::fs_rename("a.bin", "b.bin"));       // 2
    CHECK(!platform::fs_remove("old.bin"));             // cut: does nothing
    CHECK(size("old.bin") == 3 && size("b.bin") == 4);
    fake::reset();
}

NNH_TEST(fake_power_stays_off_after_the_cut_until_restored) {
    fake::reset();
    CHECK(write("keep.bin", "keep"));
    fake::cut_power_after(0);
    CHECK(!platform::fs_rename("keep.bin", "moved.bin")); // the cut
    CHECK(!write("x.bin", "x"));
    CHECK(!platform::fs_append("keep.bin", "y", 1));
    CHECK(!platform::fs_remove("keep.bin"));
    CHECK(size("keep.bin") == 4 && size("x.bin") == -1 && size("moved.bin") == -1);
    char buffer[8] = {};
    CHECK(platform::fs_read("keep.bin", 0, buffer, sizeof buffer) == 4); // reads still work
    fake::restore_power();
    CHECK(write("x.bin", "x") && size("x.bin") == 1);
    fake::cut_power_after(0);
    fake::reset(); // reset ends it too
    CHECK(write("z.bin", "z"));
    fake::reset();
}

NNH_TEST(fake_power_cut_of_an_append_keeps_half_when_torn) {
    fake::reset();
    CHECK(write("log.txt", "ab"));
    fake::cut_power_after(0);
    CHECK(!platform::fs_append("log.txt", "1234", 4));
    CHECK(size("log.txt") == 4);
    fake::reset();
}

NNH_TEST(fake_failing_writes_are_unchanged_by_power_cuts) {
    fake::reset();
    fake::fail_next_writes(1);
    CHECK(!write("a.bin", "12345678"));
    CHECK(size("a.bin") == 4);
    CHECK(write("a.bin", "12345678") && size("a.bin") == 8);
    fake::reset();
}

NNH_TEST(fake_truncate_file_cuts_a_stored_file) {
    fake::reset();
    CHECK(write("t.bin", "123456"));
    fake::truncate_file("t.bin", 2);
    CHECK(size("t.bin") == 2);
    fake::truncate_file("t.bin", 10); // not longer: unchanged
    CHECK(size("t.bin") == 2);
    fake::truncate_file("none.bin", 1);
    CHECK(size("none.bin") == -1);
    fake::reset();
}

NNH_TEST(fake_fs_log_names_each_contract_call) {
    fake::reset();
    CHECK(write("a.bin", "1"));
    CHECK(platform::fs_append("a.bin", "2", 1));
    char buffer[4];
    CHECK(platform::fs_read("a.bin", 0, buffer, sizeof buffer) == 2);
    CHECK(size("a.bin") == 2);
    CHECK(platform::fs_rename("a.bin", "b.bin"));
    CHECK(platform::fs_remove("b.bin"));
    CHECK(std::string_view(fake::fs_log()) == "WARSNX");
    fake::clear_fs_log();
    CHECK(std::string_view(fake::fs_log()).empty());
    fake::reset();
}
