// Listing data assets by type and turning references into paths (spec 014 T3.5, FR-8, FR-12).

#include <cstring>

#include "data_fixture.hpp"
#include "nnh/detail/name_hash.hpp"
#include "test.hpp"
#include "test_types.hpp"

using nnh::detail::fnv1a32;
using test_data::Enemy;
using test_data::Item;
namespace data = nnh::data;

namespace {

bool is(const char* a, const char* b) {
    return std::strcmp(a, b) == 0;
}

} // namespace

NNH_TEST(count_and_name_list_the_assets_of_a_type_in_path_order) {
    data_test::start();
    CHECK(data::count<Enemy>() == 2);
    CHECK(is(data::name<Enemy>(0), "enemies/bat"));
    CHECK(is(data::name<Enemy>(1), "enemies/goblin"));
    CHECK(data::count<Item>() == 2);
    CHECK(is(data::name<Item>(0), "items/ether"));
    CHECK(is(data::name<Item>(1), "items/potion"));
    data_test::stop();
}

NNH_TEST(names_out_of_range_are_empty) {
    data_test::start();
    CHECK(is(data::name<Enemy>(2), ""));
    CHECK(is(data::name<Enemy>(-1), ""));
    CHECK(data::ref<Item>(2).empty() && data::ref<Item>(-1).empty());
    data_test::stop();
}

NNH_TEST(ref_gives_a_reference_to_the_ith_asset) {
    data_test::start();
    CHECK(data::ref<Item>(1).id == fnv1a32("items/potion"));
    Item item;
    CHECK(data::load(data::ref<Item>(0), item).ok());
    CHECK(item.name == "Ether" && item.price == 50);
    data_test::stop();
}

NNH_TEST(find_gives_a_reference_only_to_an_asset_of_that_type) {
    data_test::start();
    CHECK(data::find<Item>("items/potion").id == fnv1a32("items/potion"));
    CHECK(data::find<Enemy>("items/potion").empty());
    CHECK(data::find<Item>("items/nothing").empty());
    CHECK(data::find<Item>("").empty() && data::find<Item>(nullptr).empty());
    data_test::stop();
}

NNH_TEST(path_turns_references_back_into_paths) {
    data_test::start();
    Enemy goblin;
    CHECK(data::load("enemies/goblin", goblin).ok());
    CHECK(is(data::path(goblin.drops[0].item), "items/potion"));
    CHECK(is(data::path(goblin.sprite), "art/goblin"));
    CHECK(is(data::path(goblin.hurt), "sfx/hurt"));
    CHECK(is(data::path(goblin.theme), "music/battle"));
    data_test::stop();
}

NNH_TEST(path_of_an_empty_unknown_or_misplaced_reference_is_empty) {
    data_test::start();
    CHECK(is(data::path(data::TextureRef{}), ""));
    CHECK(is(data::path(data::Ref<Item>{}), ""));
    CHECK(is(data::path(data::TextureRef{fnv1a32("art/missing")}), ""));
    CHECK(is(data::path(data::SoundRef{fnv1a32("art/goblin")}), "")); // a texture, not a sound
    CHECK(is(data::path(data::MusicRef{fnv1a32("sfx/hurt")}), ""));
    CHECK(is(data::path(data::TextureRef{fnv1a32("items/potion")}), ""));
    data_test::stop();
}

NNH_TEST(listing_before_init_finds_nothing) {
    data_test::stop();
    CHECK(data::count<Enemy>() == 0);
    CHECK(is(data::name<Enemy>(0), ""));
    CHECK(data::find<Enemy>("enemies/bat").empty());
    CHECK(is(data::path(data::TextureRef{fnv1a32("art/goblin")}), ""));
}

NNH_TEST(a_type_without_a_schema_has_no_assets) {
    struct NotData {
        int x = 0;
    };
    data_test::start();
    CHECK(data::count<NotData>() == 0);
    CHECK(data::count<test_data::Stats>() == 0);
    CHECK(data::find<NotData>("enemies/bat").empty());
    data_test::stop();
}
