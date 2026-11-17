// nnh::save — header, CRC-32, version, backup fallback.

#include <cstdint>
#include <cstring>

#include "fs/save_format.hpp"
#include "nnh/app.hpp"
#include "nnh/fs.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
namespace save = nnh::save;

namespace {

struct Progress {
    std::int32_t level;
    std::int32_t coins;
};

void fresh() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
}

} // namespace

NNH_TEST(crc32_matches_the_standard_check_value) {
    CHECK(nnh::detail::crc32("123456789", 9) == 0xCBF43926u);
}

NNH_TEST(saves_round_trip_with_their_version) {
    fresh();
    const Progress in{7, 1234};
    CHECK(save::write("slot1.sav", &in, sizeof in, 3).ok());
    Progress out{};
    std::uint16_t version = 0;
    const auto r = save::read("slot1.sav", &out, sizeof out, &version);
    CHECK(r.ok() && r.value() == sizeof out);
    CHECK(out.level == 7 && out.coins == 1234 && version == 3);
    nnh::shutdown();
}

NNH_TEST(corrupted_save_falls_back_to_the_backup) {
    fresh();
    const Progress first{1, 10};
    const Progress second{2, 20};
    CHECK(save::write("slot1.sav", &first, sizeof first, 1).ok());
    CHECK(save::write("slot1.sav", &second, sizeof second, 1).ok()); // first is now the .bak
    fake::corrupt_file("slot1.sav", 20);                              // flip a payload byte
    Progress out{};
    const auto r = save::read("slot1.sav", &out, sizeof out, nullptr);
    CHECK(r.ok());
    CHECK(out.level == 1 && out.coins == 10);
    nnh::shutdown();
}

NNH_TEST(corrupted_save_without_backup_is_reported) {
    fresh();
    const Progress in{5, 50};
    CHECK(save::write("solo.sav", &in, sizeof in, 1).ok());
    fake::corrupt_file("solo.sav", 2); // magic
    Progress out{};
    const auto r = save::read("solo.sav", &out, sizeof out, nullptr);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::Corrupted);
    nnh::shutdown();
}

NNH_TEST(validate_rejects_truncated_and_oversized_files) {
    std::uint8_t file[64] = {};
    const char payload[] = "abc";
    const std::size_t size = nnh::detail::build_save_header(file, sizeof payload, 1, nnh::detail::crc32(payload, sizeof payload));
    std::memcpy(file + size, payload, sizeof payload);
    CHECK(nnh::detail::validate_save(file, size + sizeof payload).ok());
    CHECK(!nnh::detail::validate_save(file, size + sizeof payload - 1).ok()); // truncated
    CHECK(!nnh::detail::validate_save(file, 5).ok());                         // shorter than header
    file[8] = 0xFF;                                                           // size field huge
    CHECK(!nnh::detail::validate_save(file, size + sizeof payload).ok());
}
