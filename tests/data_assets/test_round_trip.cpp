// Code against file (spec 014 T7.5; FR-3, FR-10, FR-11, AC-4): the fixtures of tests/data/data_assets/game,
// converted at build time by the real pipeline (nnh_add_assets with the tests' schema program, target
// nnh_test_data), loaded on the fake platform and compared field by field with what the text says.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "data_fixture.hpp"
#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"
#include "test_types.hpp"

#ifndef NNH_TEST_DATA_GAME_DIR
#error "NNH_TEST_DATA_GAME_DIR is set by tests/CMakeLists.txt"
#endif

using test_data::Element;
using test_data::Enemy;
using test_data::Item;
namespace data = nnh::data;

namespace {

std::string read_file(const std::string& path) {
    std::string bytes;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return bytes;
    char buffer[4096];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0)
        bytes.append(buffer, got);
    std::fclose(f);
    return bytes;
}

std::vector<std::string> kept, names; // the fake platform keeps pointers to paths and bytes

/// The game's packed folder as the fake platform's assets (the names of its .nnh_assets, and that list).
void start_with_the_built_assets() {
    data_test::stop();
    nnh::platform_fake::reset();
    kept.clear();
    names.clear();
    const std::string dir = NNH_TEST_DATA_GAME_DIR;
    const std::string list = read_file(dir + "/.nnh_assets");
    CHECK(!list.empty());
    for (std::size_t at = 0, lf; (lf = list.find('\n', at)) != std::string::npos; at = lf + 1) {
        const bool crlf = lf > at && list[lf - 1] == '\r'; // CMake writes CRLF on Windows
        names.push_back(list.substr(at, lf - at - (crlf ? 1 : 0)));
    }
    kept.reserve(names.size() + 1);
    kept.push_back(list);
    nnh::platform_fake::add_asset(".nnh_assets", kept.back().data(), kept.back().size());
    for (const std::string& name : names) {
        kept.push_back(read_file(dir + "/" + name));
        nnh::platform_fake::add_asset(name.c_str(), kept.back().data(), kept.back().size());
    }
    CHECK(nnh::init());
}

bool is(const char* a, const char* b) {
    return std::strcmp(a, b) == 0;
}

} // namespace

NNH_TEST(round_trip_loads_every_kind_the_text_sets) {
    start_with_the_built_assets();
    Enemy e;
    CHECK(data::load("enemies/goblin", e).ok());
    CHECK(e.name == "Goblin");
    CHECK(e.stats.hp == 300 && e.stats.attack == -3);
    CHECK(e.speed == 1.5f && e.element == Element::Fire);
    CHECK(is(data::path(e.sprite), "art/goblin") && is(data::path(e.hurt), "sfx/hurt"));
    CHECK(is(data::path(e.theme), "music/battle"));
    CHECK(e.drops.size() == 2);
    CHECK(is(data::path(e.drops.begin()[0].item), "items/potion") && e.drops.begin()[0].chance == 0.25f);
    CHECK(is(data::path(e.drops.begin()[1].item), "items/ether") && e.drops.begin()[1].chance == 0.1f);
    CHECK(e.tint.r == 0xFF && e.tint.g == 0x80 && e.tint.b == 0x40 && e.tint.a == 0xFF);
    CHECK(e.spawn.x == 3.5f && e.spawn.y == -2.0f);
    CHECK(e.flags[0] && !e.flags[1] && e.flags[2]);
    CHECK(e.level == 7);
    data_test::stop();
}

NNH_TEST(round_trip_keeps_the_structs_defaults_for_what_the_text_doesnt_set) {
    start_with_the_built_assets();
    Enemy bat; // a CRLF file setting only the name
    CHECK(data::load("enemies/bat", bat).ok());
    const Enemy defaults{};
    CHECK(bat.name == "Bat" && bat.stats.hp == defaults.stats.hp && bat.speed == defaults.speed);
    CHECK(bat.drops.empty() && bat.sprite.empty() && bat.level == defaults.level);
    CHECK(bat.flags[1] && bat.spawn.x == defaults.spawn.x);
    data_test::stop();
}

NNH_TEST(round_trip_items_escapes_separators_and_references) {
    start_with_the_built_assets();
    Item ether;
    CHECK(data::load("items/ether", ether).ok());
    CHECK(ether.name == "\xC3\x89ther" && ether.price == 1500 && ether.icon.empty());
    Item potion;
    CHECK(data::load(data::find<Item>("items/potion"), potion).ok());
    CHECK(potion.name == "Potion" && potion.price == 5 && is(data::path(potion.icon), "art/potion"));
    data_test::stop();
}

NNH_TEST(round_trip_lists_by_type_and_leaves_other_files_alone) {
    start_with_the_built_assets();
    CHECK(data::count<Enemy>() == 2 && is(data::name<Enemy>(0), "enemies/bat"));
    CHECK(data::count<Item>() == 2 && is(data::name<Item>(1), "items/potion"));
    Enemy wrong;
    CHECK(data::load("items/potion", wrong).error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(read_file(std::string(NNH_TEST_DATA_GAME_DIR) + "/notes.txt") == "Not a data asset: copied unchanged.\n");
    data_test::stop();
}
