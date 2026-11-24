// Summaries (spec 027 T4.6, FR-16): NNH_SAVE_SUMMARY loads a subset of a save type's ids with the same checks, runs
// no step (the report's version says which version wrote the file), and refuses newer files.

#include "migration_types.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::header;

NNH_TEST(save_summary_loads_its_fields_from_a_current_file) {
    mig::V3 v3;
    v3.gems = 31;
    v3.items.push_back(1);
    const Bytes b = encoded(v3);
    mig::Summary s;
    const auto r = nnh::save::decode(b.data(), b.size(), s);
    CHECK(r.ok() && s.gems == 31 && r.value().version == 3 && r.value().migrated == 0);
    CHECK(r.value().unknown == 2); // items and equipped: ids the summary doesn't declare
}

NNH_TEST(save_summary_runs_no_step_on_an_older_file) {
    mig::V1 v1;
    v1.coins = 1000;
    const Bytes b = encoded(v1);
    mig::Summary s;
    s.gems = 5;
    mig::g_log.clear();
    const auto r = nnh::save::decode(b.data(), b.size(), s);
    CHECK(r.ok() && s.gems == 0 && r.value().version == 1 && r.value().migrated == 0 && mig::g_log.length == 0);
}

NNH_TEST(save_summary_refuses_a_newer_file_and_another_type) {
    mig::Summary s;
    s.gems = 8;
    const Bytes newer = header("mig", 4);
    CHECK(nnh::save::decode(newer.data(), newer.size(), s).error().code == nnh::ErrorCode::Unsupported);
    const Bytes other = encoded(saves_test::kinds_sample());
    CHECK(nnh::save::decode(other.data(), other.size(), s).error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(s.gems == 8);
}

NNH_TEST(save_summary_validates_the_whole_file) {
    mig::V3 v3;
    v3.items.push_back(9);
    Bytes b = encoded(v3);
    b[b.size() - 4] = 0x7F; // the wire byte of field 5, which the summary doesn't declare
    mig::Summary s;
    s.gems = 8;
    const auto r = nnh::save::decode(b.data(), b.size(), s);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::Corrupted && s.gems == 8);
}
