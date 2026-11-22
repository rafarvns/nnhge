// Lookups in a validated data index (spec 014 T3.1, FR-8, FR-12): types by key and by entry, entries by group
// and path hash, paths, counts.

#include <cstring>

#include "data/index_validate.hpp"
#include "data_file.hpp"
#include "nnh/detail/name_hash.hpp"
#include "test.hpp"

using data_file::Bytes;
using nnh::detail::fnv1a32;
using nnh::detail::IndexView;
using Group = nnh::detail::idxfmt::Group;

namespace {

const Bytes& sample() {
    static const Bytes file = [] {
        data_file::IndexSpec s;
        s.types = {{"enemy", 0xAAAA0001u, {"enemies/bat", "enemies/goblin", "enemies/slime"}},
                   {"item", 0xAAAA0002u, {"items/potion"}},
                   {"abcdefghijklmnop", 0xAAAA0003u, {}}};
        s.media[0] = {"art/bat", "art/goblin"};
        s.media[1] = {"sfx/hurt"};
        s.media[2] = {"art/bat"}; // the same path as a texture: another group
        s.largest = 321;
        return data_file::index_bytes(s);
    }();
    return file;
}

IndexView view() {
    CHECK(nnh::detail::validate_index(sample().data(), sample().size()).ok());
    return IndexView{sample().data()};
}

} // namespace

NNH_TEST(index_lists_its_types_with_their_entries) {
    const IndexView v = view();
    CHECK(v.type_count() == 3);
    const auto enemy = v.type(0);
    CHECK(enemy.key_length == 5 && std::strncmp(enemy.key, "enemy", 5) == 0);
    CHECK(enemy.fingerprint == 0xAAAA0001u && enemy.first == 0 && enemy.count == 3);
    const auto item = v.type(1);
    CHECK(item.first == 3 && item.count == 1);
    CHECK(v.type(2).key_length == 16 && v.type(2).count == 0);
    CHECK(v.largest() == 321);
}

NNH_TEST(index_finds_types_by_key) {
    const IndexView v = view();
    CHECK(v.find_type("enemy") == 0);
    CHECK(v.find_type("item") == 1);
    CHECK(v.find_type("abcdefghijklmnop") == 2);
    CHECK(v.find_type("abcdefghijklmno") == -1);
    CHECK(v.find_type("enem") == -1);
    CHECK(v.find_type("enemys") == -1);
    CHECK(v.find_type("") == -1);
}

NNH_TEST(index_knows_the_type_of_each_data_entry) {
    const IndexView v = view();
    CHECK(v.type_of(0) == 0 && v.type_of(2) == 0);
    CHECK(v.type_of(3) == 1);
    CHECK(v.type_of(4) == -1); // a texture
    CHECK(v.type_of(-1) == -1 && v.type_of(100) == -1);
}

NNH_TEST(index_groups_its_entries) {
    const IndexView v = view();
    CHECK(v.entry_count() == 8);
    CHECK(v.group_first(Group::Data) == 0 && v.group_count(Group::Data) == 4);
    CHECK(v.group_first(Group::Texture) == 4 && v.group_count(Group::Texture) == 2);
    CHECK(v.group_first(Group::Sound) == 6 && v.group_count(Group::Sound) == 1);
    CHECK(v.group_first(Group::Music) == 7 && v.group_count(Group::Music) == 1);
}

NNH_TEST(index_gives_each_entry_its_path_and_hash) {
    const IndexView v = view();
    CHECK(std::strcmp(v.path(1), "enemies/goblin") == 0);
    CHECK(v.hash(1) == fnv1a32("enemies/goblin"));
    CHECK(std::strcmp(v.path(7), "art/bat") == 0);
}

NNH_TEST(index_finds_entries_by_group_and_hash) {
    const IndexView v = view();
    CHECK(v.find(Group::Data, fnv1a32("enemies/slime")) == 2);
    CHECK(v.find(Group::Data, fnv1a32("items/potion")) == 3);
    CHECK(v.find(Group::Texture, fnv1a32("art/bat")) == 4);
    CHECK(v.find(Group::Texture, fnv1a32("art/goblin")) == 5);
    CHECK(v.find(Group::Sound, fnv1a32("sfx/hurt")) == 6);
    CHECK(v.find(Group::Music, fnv1a32("art/bat")) == 7);
    CHECK(v.find(Group::Data, fnv1a32("art/bat")) == -1); // in other groups only
    CHECK(v.find(Group::Sound, fnv1a32("art/goblin")) == -1);
    CHECK(v.find(Group::Data, fnv1a32("enemies/rat")) == -1);
    CHECK(v.find(Group::Data, 0) == -1);
}

NNH_TEST(an_empty_index_finds_nothing) {
    const Bytes file = data_file::index_bytes({});
    const IndexView v{file.data()};
    CHECK(v.type_count() == 0 && v.entry_count() == 0);
    CHECK(v.find(Group::Data, 1) == -1 && v.find_type("enemy") == -1 && v.type_of(0) == -1);
}
