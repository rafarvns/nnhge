// Storing and loading files (spec 027 T5.1, FR-6, FR-9, FR-15): the file is spec 002's container (NNHS version 1)
// around the structured payload; a store makes the same file operations as nnh::save::write (one platform write); a
// load reports what it did and logs one warning line only when it used the backup, dropped or adjusted.

#include <cstring>
#include <string_view>

#include "core/crc32.hpp"
#include "migration_types.hpp"
#include "nnh/fs.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::fresh;
using saves_test::Kinds;
namespace fake = nnh::platform_fake;

namespace {

nnh::save::Buffer<Kinds> g_buffer;

} // namespace

namespace file_test {

/// Reads "mig" files of version 3 with field 5 narrowed to a u8.
struct Narrow {
    std::uint32_t gems = 0;
    std::uint8_t equipped = 0;
};
inline void narrow_step(Narrow&, const nnh::save::OldFields&) {}
NNH_SAVE_TYPE(Narrow, "mig", NNH_SAVE_VERSION(3), NNH_SAVE_STEP(1, narrow_step), NNH_SAVE_STEP(2, narrow_step),
              NNH_SAVE_FIELD(gems, 4), NNH_SAVE_FIELD(equipped, 5), NNH_SAVE_RETIRED(1, 2));

} // namespace file_test

namespace {

int console_lines() { return fake::state().console_lines; }
bool last_line_has(const char* text) { return std::strstr(fake::state().last_console_line, text) != nullptr; }

} // namespace

NNH_TEST(save_store_writes_an_nnhs_file_around_the_structured_payload) {
    fresh();
    const Kinds k = saves_test::kinds_sample();
    CHECK(nnh::save::store("slot1.sav", k, g_buffer).ok());
    std::uint8_t file[512];
    const auto r = nnh::fs::read_file("slot1.sav", file, sizeof file);
    const Bytes payload = encoded(k);
    CHECK(r.ok() && r.value() == 16 + payload.size());
    CHECK(std::memcmp(file, "NNHS", 4) == 0 && file[4] == 1 && file[5] == 0 && file[6] == 0 && file[7] == 0);
    std::uint32_t size = 0, crc = 0;
    std::memcpy(&size, file + 8, 4);
    std::memcpy(&crc, file + 12, 4);
    CHECK(size == payload.size() && crc == nnh::detail::crc32(payload.data(), payload.size()));
    CHECK(std::memcmp(file + 16, payload.data(), payload.size()) == 0);
    nnh::shutdown();
}

NNH_TEST(save_store_makes_the_file_operations_of_a_raw_save_write) {
    fresh();
    const Kinds k = saves_test::kinds_sample();
    fake::clear_fs_log();
    CHECK(nnh::save::store("slot1.sav", k, g_buffer).ok());
    CHECK(std::string_view(fake::fs_log()) == "WSN"); // the first store: write the temp file, no backup yet, rename
    fake::clear_fs_log();
    CHECK(nnh::save::store("slot1.sav", k, g_buffer).ok());
    CHECK(std::string_view(fake::fs_log()) == "WSXNN"); // as T0.2's raw save::write over an existing save
    nnh::shutdown();
}

NNH_TEST(save_load_reads_the_main_file_with_three_operations) {
    fresh();
    const Kinds k = saves_test::kinds_sample();
    CHECK(nnh::save::store("slot1.sav", k, g_buffer).ok());
    fake::clear_fs_log();
    Kinds out;
    const auto r = nnh::save::load("slot1.sav", out, g_buffer);
    CHECK(r.ok() && std::string_view(fake::fs_log()) == "SRR");
    CHECK(encoded(out) == encoded(k) && !r.value().used_backup && r.value().version == 1);
    nnh::shutdown();
}

NNH_TEST(save_load_of_a_missing_save_is_not_found_and_changes_nothing) {
    fresh();
    Kinds out = saves_test::kinds_sample();
    const auto r = nnh::save::load("none.sav", out, g_buffer);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::NotFound);
    CHECK(encoded(out) == encoded(saves_test::kinds_sample()));
    nnh::shutdown();
}

NNH_TEST(save_load_logs_nothing_when_nothing_happened) {
    fresh();
    CHECK(nnh::save::store("slot1.sav", saves_test::kinds_sample(), g_buffer).ok());
    const int before = console_lines();
    Kinds out;
    mig::V3 v3;
    nnh::save::Buffer<mig::V3> b3;
    mig::V1 v1;
    v1.coins = 50;
    CHECK(nnh::save::load("slot1.sav", out, g_buffer).ok());
    CHECK(nnh::save::store("mig.sav", v1, b3.bytes, sizeof b3.bytes).ok());
    CHECK(nnh::save::load("mig.sav", v3, b3).ok()); // migrated, nothing dropped: no line either
    CHECK(console_lines() == before);
    nnh::shutdown();
}

NNH_TEST(save_load_logs_one_warning_line_when_values_were_dropped_or_adjusted) {
    fresh();
    mig::V3 v3;
    v3.gems = 5;
    v3.equipped = 300;
    nnh::save::Buffer<mig::V3> b3;
    CHECK(nnh::save::store("mig.sav", v3, b3).ok());
    const int before = console_lines();
    file_test::Narrow narrow;
    const auto r = nnh::save::load("mig.sav", narrow, b3.bytes, sizeof b3.bytes);
    CHECK(r.ok() && r.value().dropped == 1 && narrow.gems == 5 && narrow.equipped == 0);
    CHECK(console_lines() == before + 1);
    CHECK(last_line_has("save mig.sav: loaded; unknown 1"));
    CHECK(last_line_has(nnh::build::kAssertsEnabled ? "; dropped 1 (5)" : "; dropped 1"));
    nnh::shutdown();
}

NNH_TEST(save_store_and_load_with_a_raw_buffer_and_capacity) {
    fresh();
    std::uint8_t buffer[512];
    CHECK(nnh::save::store("raw_buffer.sav", saves_test::kinds_sample(), buffer, sizeof buffer).ok());
    Kinds out;
    CHECK(nnh::save::load("raw_buffer.sav", out, buffer, sizeof buffer).ok());
    CHECK(encoded(out) == encoded(saves_test::kinds_sample()));
    nnh::shutdown();
}
