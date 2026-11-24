// Misuse (spec 027 T5.5, FR-21): a short buffer, bad paths, a string that isn't UTF-8, a null buffer — errors in
// every build, asserts where the plan says, never undefined behaviour and nothing written. (Compile-time misuse:
// the negative-compile ctests of tests/saves/compile_fail/.)

#include <cstring>

#include "../actions/actions_fixture.hpp"
#include "migration_types.hpp"
#include "nnh/fs.hpp"
#include "saves_fixture.hpp"

using saves_test::fresh;
using saves_test::Kinds;
namespace fake = nnh::platform_fake;

namespace {

nnh::save::Buffer<Kinds> g_buffer;

void store_bad_text() {
    static Kinds k;
    const char bad[] = {'\xC3', '\0'}; // a lead byte without its continuation
    k.name.assign(bad);
    (void)nnh::save::store("bad.sav", k, g_buffer);
}

} // namespace

NNH_TEST(save_store_with_a_short_buffer_is_limit_reached_and_writes_no_file) {
    fresh();
    std::uint8_t small[40];
    const auto r = nnh::save::store("slot1.sav", saves_test::kinds_sample(), small, sizeof small);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::LimitReached);
    CHECK(!nnh::fs::exists("slot1.sav") && !nnh::fs::exists("slot1.sav.tmp"));
    nnh::shutdown();
}

NNH_TEST(save_calls_refuse_bad_paths_without_touching_the_card) {
    fresh();
    const int calls = fake::state().fs_calls;
    Kinds k;
    for (const char* bad : {"../x.sav", "/x.sav", "sdmc:x.sav", "a b.sav", ""}) {
        CHECK(nnh::save::store(bad, k, g_buffer).error().code == nnh::ErrorCode::InvalidArgument);
        CHECK(nnh::save::load(bad, k, g_buffer).error().code == nnh::ErrorCode::InvalidArgument);
        CHECK(nnh::save::info(bad).error().code == nnh::ErrorCode::InvalidArgument);
        CHECK(nnh::save::remove(bad).error().code == nnh::ErrorCode::InvalidArgument);
        CHECK(!nnh::save::exists(bad));
    }
    CHECK(nnh::save::load(nullptr, k, g_buffer).error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(fake::state().fs_calls == calls);
    nnh::shutdown();
}

NNH_TEST(save_store_refuses_a_path_whose_backup_name_is_too_long) {
    fresh();
    char path[nnh::fs::kMaxPathLength + 1];
    std::memset(path, 'a', sizeof path - 1);
    path[sizeof path - 1] = '\0'; // 96 characters: valid, but "<path>.bak" isn't
    Kinds k;
    CHECK(nnh::save::store(path, k, g_buffer).error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(nnh::save::load(path, k, g_buffer).error().code == nnh::ErrorCode::InvalidArgument);
    nnh::shutdown();
}

NNH_TEST(save_calls_refuse_a_null_buffer) {
    fresh();
    Kinds k;
    CHECK(nnh::save::store("x.sav", k, nullptr, 100).error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(nnh::save::load("x.sav", k, nullptr, 100).error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(nnh::save::decode(nullptr, 10, k).error().code == nnh::ErrorCode::InvalidArgument);
    nnh::shutdown();
}

NNH_TEST(save_store_of_a_string_that_is_not_utf8_asserts_or_is_refused) {
    fresh();
    if (nnh::build::kAssertsEnabled) {
        CHECK(actions_test::crashes(store_bad_text));
    } else {
        store_bad_text();
    }
    CHECK(!nnh::fs::exists("bad.sav")); // never a file its own load would refuse
    nnh::shutdown();
}

NNH_TEST(save_load_with_a_short_buffer_is_limit_reached_and_changes_nothing) {
    fresh();
    CHECK(nnh::save::store("slot1.sav", saves_test::kinds_sample(), g_buffer).ok());
    std::uint8_t small[20];
    Kinds k;
    k.u8 = 77;
    CHECK(nnh::save::load("slot1.sav", k, small, sizeof small).error().code == nnh::ErrorCode::LimitReached);
    CHECK(k.u8 == 77);
    nnh::shutdown();
}
