// Versions and type checks (spec 027 T4.4, FR-6, FR-9, FR-14): a save of another type names both keys, a newer save
// version or format is Unsupported, a raw nnh::save payload has its own message; the struct is unchanged each time.

#include <cstring>

#include "migration_types.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::header;
using saves_test::operator+;

namespace {

bool refused(const Bytes& b, nnh::ErrorCode code, const char* message) {
    mig::V3 out;
    out.gems = 4242;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    const bool ok = !r.ok() && r.error().code == code && std::strstr(r.error().message, message) != nullptr;
    if (!ok && !r.ok()) std::printf("    got \"%s\"\n", r.error().message);
    return ok && out.gems == 4242 && out.items.empty();
}

} // namespace

NNH_TEST(save_of_another_type_is_refused_naming_both_keys) {
    CHECK(refused(encoded(saves_test::kinds_sample()), nnh::ErrorCode::InvalidArgument, "a 'kinds' save, not 'mig'"));
}

NNH_TEST(save_of_a_newer_version_is_unsupported) {
    CHECK(refused(header("mig", 5), nnh::ErrorCode::Unsupported, "saved by version 5; this game reads up to 3"));
}

NNH_TEST(save_of_a_newer_format_is_unsupported) {
    Bytes b = header("mig", 3);
    b[4] = 2;
    CHECK(refused(b, nnh::ErrorCode::Unsupported, "format 2; this engine reads 1"));
}

NNH_TEST(save_written_by_raw_save_write_has_its_own_message) {
    const Bytes raw = {7, 0, 0, 0, 9, 9, 9, 9}; // a game's struct bytes
    CHECK(refused(raw, nnh::ErrorCode::Unsupported, "a raw nnh::save file (read it with save::read)"));
}

NNH_TEST(save_with_a_bad_structure_is_corrupted_with_the_field_path) {
    CHECK(refused(header("mig", 3) + Bytes{0x46, 7, 0x84, 0x07, 1, 2, 3, 4}, nnh::ErrorCode::Corrupted,
                  "field 7: list count 900 exceeds the file"));
}

NNH_TEST(save_key_hash_and_text_must_both_match) {
    Bytes b = header("mih", 3); // a key of the same length; its own hash
    CHECK(refused(b, nnh::ErrorCode::InvalidArgument, "a 'mih' save, not 'mig'"));
}
