// Slots (spec 027 T5.2, FR-16): exists (the file or its backup), remove (the file, its backup and a temp file left by
// a cut store), info (the structured header of the main file, else of its backup), and three slots plus a settings
// file side by side.

#include <string_view>

#include "migration_types.hpp"
#include "nnh/fs.hpp"
#include "saves_fixture.hpp"

using saves_test::fresh;
namespace fake = nnh::platform_fake;

namespace slots {

struct Settings {
    std::uint8_t volume = 7;
    nnh::data::String<8> language;
};
NNH_SAVE_TYPE(Settings, "settings", NNH_SAVE_FIELD(volume, 1), NNH_SAVE_FIELD(language, 2));

} // namespace slots

namespace {

nnh::save::Buffer<mig::V3> g_buffer;

void store(const char* path, std::uint32_t gems) {
    mig::V3 v;
    v.gems = gems;
    CHECK(nnh::save::store(path, v, g_buffer).ok());
}

} // namespace

NNH_TEST(save_exists_sees_the_file_or_its_backup) {
    fresh();
    CHECK(!nnh::save::exists("slot1.sav"));
    store("slot1.sav", 1);
    CHECK(nnh::save::exists("slot1.sav"));
    store("slot1.sav", 2);
    CHECK(nnh::fs::remove("slot1.sav").ok()); // only the backup is left
    CHECK(nnh::save::exists("slot1.sav"));
    CHECK(!nnh::save::exists("../slot1.sav") && !nnh::save::exists(nullptr));
    nnh::shutdown();
}

NNH_TEST(save_remove_deletes_the_file_its_backup_and_a_temp_file) {
    fresh();
    store("slot1.sav", 1);
    store("slot1.sav", 2);
    const std::uint8_t junk[3] = {1, 2, 3};
    CHECK(nnh::fs::write_file("slot1.sav.tmp", junk, sizeof junk).ok()); // a store cut before its rename
    CHECK(nnh::save::remove("slot1.sav").ok());
    CHECK(!nnh::fs::exists("slot1.sav") && !nnh::fs::exists("slot1.sav.bak") && !nnh::fs::exists("slot1.sav.tmp"));
    CHECK(!nnh::save::exists("slot1.sav"));
    CHECK(nnh::save::remove("slot1.sav").ok()); // nothing left: still fine
    CHECK(nnh::save::remove("/abs.sav").error().code == nnh::ErrorCode::InvalidArgument);
    nnh::shutdown();
}

NNH_TEST(save_remove_reports_a_file_it_could_not_delete) {
    fresh();
    store("slot1.sav", 1);
    fake::cut_power_after(0, false); // every removal fails
    const auto r = nnh::save::remove("slot1.sav");
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::IoError);
    fake::restore_power();
    nnh::shutdown();
}

NNH_TEST(save_info_reads_the_headers_with_one_size_and_one_read) {
    fresh();
    store("slot1.sav", 1);
    fake::clear_fs_log();
    const auto r = nnh::save::info("slot1.sav");
    CHECK(r.ok() && std::string_view(fake::fs_log()) == "SR");
    CHECK(std::string_view(r.value().key) == "mig" && r.value().version == 3 && !r.value().from_backup);
    CHECK(r.value().key_hash == nnh::detail::save_key_hash("mig"));
    CHECK(r.value().size == saves_test::encoded(mig::V3{.gems = 1}).size());
    nnh::shutdown();
}

NNH_TEST(save_info_falls_back_to_the_backup_header) {
    fresh();
    store("slot1.sav", 1);
    store("slot1.sav", 2);
    fake::truncate_file("slot1.sav", 8);
    const auto r = nnh::save::info("slot1.sav");
    CHECK(r.ok() && r.value().from_backup && r.value().version == 3);
    CHECK(nnh::save::info("none.sav").error().code == nnh::ErrorCode::NotFound);
    nnh::shutdown();
}

NNH_TEST(save_info_recognises_a_raw_file) {
    fresh();
    const std::uint8_t raw[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(nnh::save::write("raw.sav", raw, sizeof raw, 7).ok());
    const auto r = nnh::save::info("raw.sav");
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::Unsupported);
    nnh::shutdown();
}

NNH_TEST(save_slots_and_settings_live_side_by_side) {
    fresh();
    store("slot1.sav", 11);
    store("slot2.sav", 22);
    store("slot3.sav", 33);
    slots::Settings settings;
    settings.volume = 3;
    settings.language.assign("pt");
    nnh::save::Buffer<slots::Settings> sb;
    CHECK(nnh::save::store("settings.sav", settings, sb).ok());
    mig::Summary s;
    CHECK(nnh::save::load("slot2.sav", s, g_buffer.bytes, sizeof g_buffer.bytes).ok() && s.gems == 22);
    CHECK(nnh::save::load("slot3.sav", s, g_buffer.bytes, sizeof g_buffer.bytes).ok() && s.gems == 33);
    slots::Settings back;
    CHECK(nnh::save::load("settings.sav", back, sb).ok() && back.volume == 3 && back.language == "pt");
    CHECK(nnh::save::load("settings.sav", s, g_buffer.bytes, sizeof g_buffer.bytes).error().code ==
          nnh::ErrorCode::InvalidArgument); // a settings file isn't a slot
    nnh::shutdown();
}
