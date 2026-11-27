// The data kind of hot reload (spec 031 T2.7, FR-11, FR-14, FR-16, FR-18): count/name/find/path after an add, a remove
// and a rename; the game's structs untouched until it loads them again; a data file for another struct refused ("rebuild
// and restart"); an index larger than the old block.

#include <algorithm>
#include <cstring>

#include "../reload/reload_fixture.hpp"
#include "data/data_index.hpp"
#include "data_fixture.hpp"
#include "nnh/memory.hpp"
#include "nnh/data.hpp"
#include "test_types.hpp"

using namespace reload_test;
using data_file::Writer;

namespace {

/// The standard data set, attached to the PC, the index loaded (the kind registers then).
void start_data() {
    data_test::start();
    fake::remote_connect();
    send(remote_test::hello());
    CHECK(nnh::frame());
    CHECK(nnh::data::count<test_data::Enemy>() == 2);
}

data_file::Bytes enemy(const char* name, std::uint32_t fingerprint = data_test::enemy_fingerprint()) {
    return Writer(fingerprint).fields(1).field(0).str(name).bytes();
}

data_file::Bytes index_with(std::vector<std::string> enemies, std::uint32_t largest) {
    data_file::IndexSpec spec = data_test::standard_index();
    spec.types[0].paths = std::move(enemies);
    spec.largest = largest;
    return data_file::index_bytes(spec);
}

const char kAssets[] = ".nnh_data\nenemies/bat.enemy\nenemies/goblin.enemy\nenemies/orc.enemy\nitems/ether.item\n"
                       "items/potion.item\n";

} // namespace

NNH_TEST(data_reload_an_added_asset_is_counted_found_and_loaded) {
    RELOAD_DEV_ONLY();
    start_data();
    const data_file::Bytes orc = enemy("Orc");
    const data_file::Bytes index = index_with({"enemies/bat", "enemies/goblin", "enemies/orc"}, 512);
    const Put batch[] = {Put{"enemies/orc.enemy", orc.data(), orc.size()}, Put{".nnh_data", index.data(), index.size()},
                         Put{".nnh_assets", kAssets, sizeof kAssets - 1}};
    send_batch(1, batch, 3);
    CHECK(nnh::frame());
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(nnh::data::count<test_data::Enemy>() == 3);
    CHECK(std::strcmp(nnh::data::name<test_data::Enemy>(2), "enemies/orc") == 0);
    CHECK(nnh::data::find<test_data::Enemy>("enemies/orc").id != 0);
    test_data::Enemy e;
    CHECK(nnh::data::load("enemies/orc", e).ok());
    CHECK(std::strcmp(e.name.c_str(), "Orc") == 0);
    data_test::stop();
}

NNH_TEST(data_reload_a_borrowed_name_survives_one_reload) {
    RELOAD_DEV_ONLY();
    start_data();
    nnh::platform_fake::set_alloc_fill(0xAA); // a freed and reused block would show garbage
    const char* bat = nnh::data::name<test_data::Enemy>(0); // borrowed from the index block
    CHECK(std::strcmp(bat, "enemies/bat") == 0);
    const data_file::Bytes orc = enemy("Orc");
    const data_file::Bytes index = index_with({"enemies/bat", "enemies/goblin", "enemies/orc"}, 512);
    const Put batch[] = {Put{"enemies/orc.enemy", orc.data(), orc.size()}, Put{".nnh_data", index.data(), index.size()},
                         Put{".nnh_assets", kAssets, sizeof kAssets - 1}};
    send_batch(1, batch, 3);
    CHECK(nnh::frame());
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(nnh::mem::tag_stats(nnh::detail::data_memory_tag()).count >= 2); // the previous block is still there
    CHECK(std::strcmp(bat, "enemies/bat") == 0); // still readable until the next batch (review S3)
    nnh::platform_fake::set_alloc_fill(-1);
    data_test::stop();
}

NNH_TEST(data_reload_the_games_struct_is_untouched_until_it_loads_again) {
    RELOAD_DEV_ONLY();
    start_data();
    test_data::Enemy bat;
    CHECK(nnh::data::load("enemies/bat", bat).ok());
    CHECK(std::strcmp(bat.name.c_str(), "Bat") == 0);
    const data_file::Bytes vampire = enemy("Vampire bat");
    const Put batch[] = {Put{"enemies/bat.enemy", vampire.data(), vampire.size()}};
    send_batch(1, batch, 1);
    CHECK(nnh::frame());
    CHECK(nnh::assets::reloaded("enemies/bat"));
    CHECK(std::strcmp(bat.name.c_str(), "Bat") == 0); // announced, not rewritten (FR-11)
    CHECK(nnh::data::load("enemies/bat", bat).ok());
    CHECK(std::strcmp(bat.name.c_str(), "Vampire bat") == 0);
    data_test::stop();
}

NNH_TEST(data_reload_a_file_for_another_struct_is_refused) {
    RELOAD_DEV_ONLY();
    start_data();
    Lines warn("reload refused");
    const data_file::Bytes stale = enemy("Stale", data_test::enemy_fingerprint() ^ 1u);
    const Put batch[] = {Put{"enemies/bat.enemy", stale.data(), stale.size()}};
    send_batch(1, batch, 1);
    CHECK(nnh::frame());
    CHECK(nnh::assets::reload_count() == 0);
    CHECK(warn.count == 1);
    CHECK(std::strstr(warn.last, "enemies/bat.enemy: made for another struct: rebuild and restart") != nullptr);
    CHECK(records().last_refused.stage == wire::ReloadStage::Restart);
    test_data::Enemy bat;
    CHECK(nnh::data::load("enemies/bat", bat).ok() && std::strcmp(bat.name.c_str(), "Bat") == 0);
    data_test::stop();
}

NNH_TEST(data_reload_a_removed_and_a_renamed_asset) {
    RELOAD_DEV_ONLY();
    start_data();
    // goblin → orc: the index lists bat and orc
    const data_file::Bytes orc = enemy("Orc");
    const data_file::Bytes index = index_with({"enemies/bat", "enemies/orc"}, 512);
    static const char assets[] = ".nnh_data\nenemies/bat.enemy\nenemies/orc.enemy\nitems/ether.item\nitems/potion.item\n";
    const Put batch[] = {Put{"enemies/orc.enemy", orc.data(), orc.size()}, removed("enemies/goblin.enemy"),
                         Put{".nnh_data", index.data(), index.size()}, Put{".nnh_assets", assets, sizeof assets - 1}};
    send_batch(1, batch, 4);
    CHECK(nnh::frame());
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(nnh::data::count<test_data::Enemy>() == 2);
    CHECK(nnh::data::find<test_data::Enemy>("enemies/goblin").id == 0);
    test_data::Enemy e;
    CHECK(!nnh::data::load("enemies/goblin", e).ok());
    CHECK(nnh::data::load("enemies/orc", e).ok());
    data_test::stop();
}

NNH_TEST(data_reload_an_index_larger_than_the_old_block) {
    RELOAD_DEV_ONLY();
    start_data();
    std::vector<std::string> many = {"enemies/bat", "enemies/goblin"};
    std::string assets = ".nnh_data\nenemies/bat.enemy\nenemies/goblin.enemy\n";
    static data_file::Bytes extra[40];
    Put batch[44];
    int n = 0;
    for (int i = 0; i < 40; ++i) {
        char path[32];
        std::snprintf(path, sizeof path, "enemies/z%02d", i);
        many.emplace_back(path);
        assets += std::string(path) + ".enemy\n";
    }
    std::sort(many.begin(), many.end());
    static std::string paths[40];
    for (int i = 0; i < 40; ++i) {
        paths[i] = many[2 + i] + ".enemy";
        extra[i] = enemy("Z");
        batch[n++] = Put{paths[i].c_str(), extra[i].data(), extra[i].size()};
    }
    assets += "items/ether.item\nitems/potion.item\n";
    static data_file::Bytes index;
    index = index_with(many, 512);
    static std::string asset_text;
    asset_text = assets;
    batch[n++] = Put{".nnh_data", index.data(), index.size()};
    batch[n++] = Put{".nnh_assets", asset_text.data(), asset_text.size()};
    send_batch(1, batch, n);
    frames(2);
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(nnh::data::count<test_data::Enemy>() == 42);
    data_test::stop();
}
