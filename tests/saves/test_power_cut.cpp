// Power cuts (spec 027 T5.3, AC-3, constitution X): a store cut after each of its file operations in turn (the write
// torn or not, each rename or removal not done), for a first store and for a store over an existing save; the next
// load gives the new or the previous values, never Corrupted, never NotFound once a save existed; and another store
// after the cut works.

#include "migration_types.hpp"
#include "saves_fixture.hpp"

using saves_test::fresh;
namespace fake = nnh::platform_fake;

namespace {

nnh::save::Buffer<mig::V3> g_buffer;

bool store(std::uint32_t gems) {
    mig::V3 v;
    v.gems = gems;
    return nnh::save::store("cut.sav", v, g_buffer).ok();
}

/// The gems loaded, or the error code's negative value.
long long load() {
    mig::V3 v;
    const auto r = nnh::save::load("cut.sav", v, g_buffer);
    return r.ok() ? static_cast<long long>(v.gems) : -static_cast<long long>(r.error().code);
}

/// Stores `previous` saves (0: none), cuts the power after `ops` mutating calls of the next store of 100, and
/// checks what a load finds, then that a store after the cut works.
void cut_case(int previous, int ops, bool torn) {
    fresh();
    for (int i = 1; i <= previous; ++i) CHECK(store(static_cast<std::uint32_t>(i)));
    fake::cut_power_after(ops, torn);
    const bool stored = store(100);
    fake::restore_power();
    const long long loaded = load();
    if (previous == 0) {
        // nothing existed: the new save or still none
        CHECK(loaded == 100 || loaded == -static_cast<long long>(nnh::ErrorCode::NotFound));
    } else {
        CHECK(loaded == 100 || loaded == previous); // never Corrupted, never NotFound
    }
    CHECK(!stored || loaded == 100);
    CHECK(store(200) && load() == 200);
    nnh::shutdown();
}

} // namespace

NNH_TEST(save_power_cut_during_a_first_store_leaves_none_or_the_new_save) {
    for (int ops = 0; ops <= 4; ++ops) {
        cut_case(0, ops, true);
        cut_case(0, ops, false);
    }
}

NNH_TEST(save_power_cut_over_one_save_leaves_the_old_or_the_new_save) {
    for (int ops = 0; ops <= 4; ++ops) {
        cut_case(1, ops, true);
        cut_case(1, ops, false);
    }
}

NNH_TEST(save_power_cut_over_a_save_and_its_backup_leaves_the_old_or_the_new_save) {
    for (int ops = 0; ops <= 4; ++ops) {
        cut_case(2, ops, true);
        cut_case(2, ops, false);
    }
}

NNH_TEST(save_power_cut_between_the_two_renames_loads_the_backup) {
    fresh();
    CHECK(store(1));
    CHECK(store(2));
    fake::cut_power_after(3); // write temp, remove .bak, rename main -> .bak; the last rename is cut
    CHECK(!store(100));
    fake::restore_power();
    mig::V3 v;
    const auto r = nnh::save::load("cut.sav", v, g_buffer);
    CHECK(r.ok() && r.value().used_backup && v.gems == 2);
    nnh::shutdown();
}
