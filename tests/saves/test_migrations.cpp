// Versions and migration steps (spec 027 T4.5, FR-12): an older file runs the steps from its version in order after
// the automatic load, each step sees the loaded struct and the file's old fields; a current file runs none; a record
// whose step is missing asserts in Debug/Development and skips it in Release.

#include <string_view>

#include "../actions/actions_fixture.hpp"
#include "migration_types.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;

// The steps (declared in migration_types.hpp).
void mig::v2_from_v1(V2& value, const nnh::save::OldFields& old) {
    g_log.add('1');
    std::uint16_t weapon = 0;
    if (old.get(nnh::save::path(2), weapon)) value.items.push_back(weapon);
}

void mig::v3_from_v1(V3& value, const nnh::save::OldFields& old) {
    g_log.add('1');
    std::uint16_t weapon = 0;
    if (old.get(nnh::save::path(2), weapon)) value.items.push_back(weapon);
}

void mig::v3_from_v2(V3& value, const nnh::save::OldFields& old) {
    g_log.add('2');
    g_log.add(static_cast<char>('0' + value.items.size())); // what the automatic load (or step 1) left
    std::uint32_t coins = 0;
    if (old.get(nnh::save::path(1), coins)) value.gems = coins / 10;
    if (!value.items.empty()) value.equipped = value.items[0];
}

namespace {

Bytes v1_file() {
    mig::V1 v1;
    v1.coins = 1234;
    v1.weapon = 77;
    return encoded(v1);
}

Bytes v2_file() {
    mig::V2 v2;
    v2.coins = 560;
    v2.items.push_back(5);
    v2.items.push_back(6);
    return encoded(v2);
}

nnh::save::Report decode_v3(const Bytes& b, mig::V3& out) {
    mig::g_log.clear();
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok());
    return r.ok() ? r.value() : nnh::save::Report{};
}

Bytes g_v1;
void decode_with_a_missing_step() {
    static nnh::detail::SaveStep steps[2];
    steps[0] = nnh::detail::save_info_of<mig::V3>().steps[0];
    steps[1] = {2, nullptr}; // a hand-built record: the step from v2 has no function
    nnh::detail::SaveTypeInfo broken = nnh::detail::save_info_of<mig::V3>();
    broken.steps = steps;
    mig::V3 out;
    mig::g_log.clear();
    const auto r = nnh::detail::save_decode(broken, g_v1.data(), g_v1.size(), &out);
    CHECK(r.ok() && r.value().migrated == 1 && mig::g_log.length == 1); // Release: skipped, never undefined
}

} // namespace

NNH_TEST(save_migration_runs_steps_one_and_two_for_a_v1_file) {
    mig::V3 out;
    const nnh::save::Report r = decode_v3(v1_file(), out);
    CHECK(r.version == 1 && r.migrated == 2);
    CHECK(std::string_view(mig::g_log.text) == "121"); // step 1, then step 2 seeing the item step 1 added
    CHECK(out.gems == 123 && out.items.size() == 1 && out.items[0] == 77 && out.equipped == 77);
}

NNH_TEST(save_migration_runs_only_step_two_for_a_v2_file) {
    mig::V3 out;
    const nnh::save::Report r = decode_v3(v2_file(), out);
    CHECK(r.version == 2 && r.migrated == 1);
    CHECK(std::string_view(mig::g_log.text) == "22"); // step 2 saw the two items of the automatic load
    CHECK(out.gems == 56 && out.items.size() == 2 && out.equipped == 5);
}

NNH_TEST(save_migration_runs_no_step_for_a_current_file) {
    mig::V3 in;
    in.gems = 9;
    in.items.push_back(3);
    in.equipped = 3;
    mig::V3 out;
    const nnh::save::Report r = decode_v3(encoded(in), out);
    CHECK(r.version == 3 && r.migrated == 0 && mig::g_log.length == 0);
    CHECK(encoded(out) == encoded(in));
}

NNH_TEST(save_migration_steps_run_for_the_middle_version_too) {
    mig::V2 out;
    mig::g_log.clear();
    const Bytes b = v1_file();
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && r.value().migrated == 1 && out.coins == 1234 && out.items.size() == 1 && out.items[0] == 77);
    CHECK(r.value().unknown == 0); // field 2 is retired in v2: skipped silently
}

NNH_TEST(save_migration_with_a_missing_step_asserts_or_is_skipped) {
    g_v1 = v1_file();
    if (nnh::build::kAssertsEnabled) {
        CHECK(actions_test::crashes(decode_with_a_missing_step));
    } else {
        decode_with_a_missing_step();
    }
}
