// nnh::data::load (spec 014 T3.4, FR-8, FR-11): by path and by reference, into T{} plus the file's values;
// every error leaves `out` unchanged.

#include <cstring>

#include "data/data_format.hpp"
#include "data_fixture.hpp"
#include "nnh/app.hpp"
#include "nnh/detail/name_hash.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"
#include "test_types.hpp"

namespace fake = nnh::platform_fake;
using nnh::ErrorCode;
using nnh::detail::fnv1a32;
using test_data::Enemy;
using test_data::Item;
namespace fmt = nnh::detail::datafmt;

namespace {

/// A struct full of values no file sets, to see that a failed load leaves it alone.
Enemy marked() {
    Enemy e;
    e.name.assign("marked");
    e.level = 42;
    e.stats.hp = 777;
    return e;
}

/// Field by field (padding bytes may differ).
bool same(const Enemy& a, const Enemy& b) {
    bool drops = a.drops.size() == b.drops.size();
    for (int i = 0; drops && i < a.drops.size(); ++i) {
        drops = a.drops[i].item == b.drops[i].item && a.drops[i].chance == b.drops[i].chance;
    }
    return a.name == b.name && a.stats.hp == b.stats.hp && a.stats.attack == b.stats.attack &&
           a.speed == b.speed && a.element == b.element && a.sprite == b.sprite && a.hurt == b.hurt &&
           drops && a.tint.r == b.tint.r && a.tint.g == b.tint.g && a.tint.b == b.tint.b &&
           a.tint.a == b.tint.a && a.spawn.x == b.spawn.x && a.spawn.y == b.spawn.y &&
           a.flags[0] == b.flags[0] && a.flags[1] == b.flags[1] && a.flags[2] == b.flags[2] &&
           a.level == b.level && a.theme == b.theme;
}

bool unchanged(const Enemy& e) {
    return same(e, marked());
}

template <class T, class P> ErrorCode load_error(P what, T& out) {
    auto r = nnh::data::load(what, out);
    return r ? ErrorCode::None : r.error().code;
}

bool last_log_has(const char* text) {
    return std::strstr(fake::state().last_console_line, text) != nullptr;
}

} // namespace

NNH_TEST(load_reads_every_value_of_a_data_asset) {
    data_test::start();
    Enemy e = marked();
    CHECK(nnh::data::load("enemies/goblin", e).ok());
    CHECK(e.name == "Goblin");
    CHECK(e.stats.hp == 30 && e.stats.attack == -3);
    CHECK(e.speed == 1.5f && e.element == test_data::Element::Fire);
    CHECK(e.sprite.id == fnv1a32("art/goblin") && e.hurt.id == fnv1a32("sfx/hurt"));
    CHECK(e.drops.size() == 1 && e.drops[0].item.id == fnv1a32("items/potion") && e.drops[0].chance == 0.25f);
    CHECK(e.tint.r == 10 && e.tint.g == 20 && e.tint.b == 30 && e.tint.a == 40);
    CHECK(e.spawn.x == 3 && e.spawn.y == -4);
    CHECK(e.flags[0] && !e.flags[1] && e.flags[2]);
    CHECK(e.level == 7 && e.theme.id == fnv1a32("music/battle"));
    data_test::stop();
}

NNH_TEST(load_starts_from_the_structs_defaults) {
    data_test::start();
    Enemy e = marked();
    CHECK(nnh::data::load("enemies/bat", e).ok());
    Enemy expected;
    expected.name.assign("Bat");
    CHECK(same(e, expected));
    data_test::stop();
}

NNH_TEST(load_follows_a_reference) {
    data_test::start();
    Enemy goblin;
    CHECK(nnh::data::load("enemies/goblin", goblin).ok());
    Item drop;
    CHECK(nnh::data::load(goblin.drops[0].item, drop).ok());
    CHECK(drop.name == "Potion" && drop.price == 5 && drop.icon.id == fnv1a32("art/potion"));
    data_test::stop();
}

NNH_TEST(load_refuses_an_empty_or_unknown_reference) {
    data_test::start();
    Item item;
    item.price = 99;
    CHECK(load_error(nnh::data::Ref<Item>{}, item) == ErrorCode::NotFound);
    CHECK(load_error(nnh::data::Ref<Item>{fnv1a32("items/missing")}, item) == ErrorCode::NotFound);
    CHECK(load_error(nnh::data::Ref<Item>{fnv1a32("enemies/bat")}, item) == ErrorCode::InvalidArgument);
    CHECK(item.price == 99);
    data_test::stop();
}

NNH_TEST(load_refuses_missing_names_and_bad_paths) {
    data_test::start();
    Enemy e = marked();
    CHECK(load_error("enemies/ghost", e) == ErrorCode::NotFound);
    CHECK(last_log_has("enemies/ghost"));
    CHECK(load_error("enemies/goblin.enemy", e) == ErrorCode::NotFound); // no extension in names
    CHECK(load_error("../enemies/goblin", e) == ErrorCode::InvalidArgument);
    CHECK(load_error("", e) == ErrorCode::InvalidArgument);
    CHECK(load_error(static_cast<const char*>(nullptr), e) == ErrorCode::InvalidArgument);
    CHECK(unchanged(e));
    data_test::stop();
}

NNH_TEST(load_refuses_an_asset_of_another_type) {
    data_test::start();
    Enemy e = marked();
    CHECK(load_error("items/potion", e) == ErrorCode::InvalidArgument);
    CHECK(last_log_has("is an item, not an enemy"));
    CHECK(unchanged(e));
    data_test::stop();
}

NNH_TEST(load_refuses_a_type_without_a_schema) {
    struct NotData {
        int x = 1;
    };
    data_test::start();
    NotData n;
    CHECK(load_error("enemies/bat", n) == ErrorCode::InvalidArgument);
    CHECK(n.x == 1);
    test_data::Stats keyless; // a nested-only struct has no assets
    CHECK(load_error("enemies/bat", keyless) == ErrorCode::InvalidArgument);
    data_test::stop();
}

NNH_TEST(load_refuses_a_file_converted_for_another_version_of_the_type) {
    data_test::Files f = data_test::standard();
    data_file::put32(f.goblin, fmt::kFingerprintAt, data_test::enemy_fingerprint() ^ 1);
    data_file::seal(f.goblin);
    data_test::start(f);
    Enemy e = marked();
    auto r = nnh::data::load("enemies/goblin", e);
    CHECK(!r && r.error().code == ErrorCode::Corrupted);
    CHECK(std::strstr(r.error().message, "Enemy") != nullptr);
    CHECK(std::strstr(r.error().message, "rebuild the assets") != nullptr);
    CHECK(last_log_has("converted for another version of Enemy: rebuild the assets"));
    CHECK(unchanged(e));
    data_test::stop();
}

NNH_TEST(load_refuses_damaged_files) {
    data_test::Files f = data_test::standard();
    f.goblin.back() ^= 0x40; // CRC mismatch
    f.bat[fmt::kVersionAt] = 9;
    data_file::seal(f.bat);
    data_test::start(f);
    Enemy e = marked();
    CHECK(load_error("enemies/goblin", e) == ErrorCode::Corrupted);
    CHECK(load_error("enemies/bat", e) == ErrorCode::Unsupported);
    CHECK(unchanged(e));
    data_test::stop();
}

NNH_TEST(load_refuses_a_file_larger_than_the_index_says) {
    data_test::Files f = data_test::standard();
    f.bat = data_test::standard().goblin;
    f.bat.resize(f.bat.size() + 8, 0); // bigger than the largest the index recorded
    data_file::seal(f.bat);
    data_test::start(f);
    Enemy e = marked();
    CHECK(load_error("enemies/bat", e) == ErrorCode::Corrupted);
    CHECK(unchanged(e));
    data_test::stop();
}

NNH_TEST(load_reports_an_asset_missing_from_the_game) {
    data_test::Files f = data_test::standard();
    data_file::IndexSpec spec = data_test::standard_index();
    spec.types[0].paths.push_back("enemies/rat"); // in the index, but no enemies/rat.enemy
    spec.largest = static_cast<std::uint32_t>(f.goblin.size());
    f.index = data_file::index_bytes(spec);
    data_test::start(f);
    Enemy e = marked();
    CHECK(load_error("enemies/rat", e) == ErrorCode::NotFound);
    CHECK(unchanged(e));
    data_test::stop();
}

NNH_TEST(load_before_init_is_unsupported) {
    data_test::stop();
    Enemy e = marked();
    CHECK(load_error("enemies/goblin", e) == ErrorCode::Unsupported);
    CHECK(unchanged(e));
}

NNH_TEST(load_refuses_a_name_that_only_shares_its_hash_with_an_asset) {
    // "enemies/e398497" and "enemies/e1059030" have the same FNV-1a 32 (EC4D808F): the index knows the first.
    static_assert(fnv1a32("enemies/e398497") == fnv1a32("enemies/e1059030"));
    data_test::Files f = data_test::standard();
    data_file::IndexSpec spec = data_test::standard_index();
    spec.types[0].paths = {"enemies/bat", "enemies/e398497", "enemies/goblin"};
    spec.largest = static_cast<std::uint32_t>(f.goblin.size());
    f.index = data_file::index_bytes(spec);
    data_test::start(f);
    Enemy e = marked();
    CHECK(nnh::data::load("enemies/e398497", e).ok()); // the asset the index knows loads
    e = marked();
    CHECK(load_error("enemies/e1059030", e) == ErrorCode::NotFound);
    CHECK(nnh::data::find<Enemy>("enemies/e1059030").empty());
    CHECK(unchanged(e));
    data_test::stop();
}
