// Backups (spec 027 T5.1, FR-7, AC-3): a damaged main file — a CRC-valid but broken structure, a bit flip, a
// truncation — or a missing one loads from `<path>.bak` and the report says so; a save of another type or a newer
// version never tries the backup; both unusable is Corrupted with the struct unchanged.

#include <cstring>
#include <string_view>

#include "migration_types.hpp"
#include "nnh/fs.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::fresh;
using saves_test::header;
using saves_test::operator+;
namespace fake = nnh::platform_fake;

namespace {

nnh::save::Buffer<mig::V3, 64> g_buffer;

/// Two stores: `first` ends up in mig.sav.bak, `second` in mig.sav.
void two_saves(std::uint32_t first, std::uint32_t second) {
    mig::V3 v;
    v.gems = first;
    CHECK(nnh::save::store("mig.sav", v, g_buffer).ok());
    v.gems = second;
    CHECK(nnh::save::store("mig.sav", v, g_buffer).ok());
}

/// Loads mig.sav; returns the gems (0 on failure) and the report.
std::uint32_t load_gems(nnh::save::Report* report = nullptr, nnh::ErrorCode* code = nullptr) {
    mig::V3 v;
    v.gems = 4242;
    const auto r = nnh::save::load("mig.sav", v, g_buffer);
    if (code != nullptr) *code = r.ok() ? nnh::ErrorCode::None : r.error().code;
    if (report != nullptr && r.ok()) *report = r.value();
    return v.gems;
}

/// Writes `payload` with a valid NNHS CRC (through raw nnh::save::write) as mig.sav, over the existing one.
void replace_main(const Bytes& payload) {
    CHECK(nnh::fs::remove("mig.sav").ok());
    CHECK(nnh::save::write("mig.sav", payload.data(), payload.size(), 1).ok());
    // save::write moved nothing (no main file existed): the .bak is still the first save
}

} // namespace

NNH_TEST(save_backup_is_used_for_a_crc_valid_file_with_a_broken_structure) {
    fresh();
    two_saves(10, 20);
    replace_main(header("mig", 3) + Bytes{0x46, 7, 0x84, 0x07, 1, 2, 3, 4}); // count 900, 4 bytes
    nnh::save::Report r;
    const int before = fake::state().console_lines;
    CHECK(load_gems(&r) == 10 && r.used_backup);
    CHECK(fake::state().console_lines == before + 1);
    CHECK(std::strstr(fake::state().last_console_line, "backup used (mig.sav: field 7: list count 900") != nullptr);
    nnh::shutdown();
}

NNH_TEST(save_backup_is_used_for_a_bit_flip_and_a_truncation) {
    fresh();
    two_saves(10, 20);
    fake::corrupt_file("mig.sav", 30);
    nnh::save::Report r;
    CHECK(load_gems(&r) == 10 && r.used_backup);
    two_saves(30, 40); // the damaged file went to .bak, then 30 replaced it: .bak = 30, main = 40
    fake::truncate_file("mig.sav", 20);
    CHECK(load_gems(&r) == 30 && r.used_backup);
    nnh::shutdown();
}

NNH_TEST(save_backup_is_used_when_the_main_file_is_missing) {
    fresh();
    two_saves(10, 20);
    CHECK(nnh::fs::remove("mig.sav").ok());
    nnh::save::Report r;
    CHECK(load_gems(&r) == 10 && r.used_backup && r.version == 3);
    nnh::shutdown();
}

NNH_TEST(save_backup_is_not_tried_for_another_type_or_a_newer_version) {
    fresh();
    two_saves(10, 20);
    replace_main(header("kinds", 1));
    fake::clear_fs_log();
    nnh::ErrorCode code{};
    CHECK(load_gems(nullptr, &code) == 4242 && code == nnh::ErrorCode::InvalidArgument);
    CHECK(std::string_view(fake::fs_log()) == "SRR"); // the backup was never read
    replace_main(header("mig", 9));
    fake::clear_fs_log();
    CHECK(load_gems(nullptr, &code) == 4242 && code == nnh::ErrorCode::Unsupported);
    CHECK(std::string_view(fake::fs_log()) == "SRR");
    replace_main(Bytes{1, 2, 3, 4, 5}); // a raw nnh::save file
    CHECK(load_gems(nullptr, &code) == 4242 && code == nnh::ErrorCode::Unsupported);
    nnh::shutdown();
}

NNH_TEST(save_backup_both_unusable_is_corrupted_and_changes_nothing) {
    fresh();
    two_saves(10, 20);
    fake::corrupt_file("mig.sav", 30);
    fake::corrupt_file("mig.sav.bak", 30);
    nnh::ErrorCode code{};
    CHECK(load_gems(nullptr, &code) == 4242 && code == nnh::ErrorCode::Corrupted);
    CHECK(nnh::fs::remove("mig.sav").ok());
    CHECK(load_gems(nullptr, &code) == 4242 && code == nnh::ErrorCode::Corrupted); // missing main, damaged backup
    nnh::shutdown();
}

NNH_TEST(save_backup_of_a_buffer_too_small_is_not_tried) {
    fresh();
    two_saves(10, 20);
    mig::V3 v;
    std::uint8_t small[20];
    const auto r = nnh::save::load("mig.sav", v, small, sizeof small);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::LimitReached);
    nnh::shutdown();
}
