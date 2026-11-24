// The migration steps' view of the file (spec 027 T4.5, FR-13, FR-21): retired, unknown and dropped values are all
// readable; nested paths and items; a struct region decoded into a save struct by its ids; kinds that can't hold the
// value answer false and leave `out` unchanged; the "keep the old struct" pattern.

#include "migration_types.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::Pair;

namespace view {

struct Item {
    std::uint16_t kind = 0;
    nnh::data::String<6> label;
};
NNH_SAVE_STRUCT(Item, NNH_SAVE_FIELD(kind, 1), NNH_SAVE_FIELD(label, 2));

struct Old {
    std::uint32_t coins = 300;
    Pair pair{4, 5};
    nnh::data::List<Item, 8> items;
    nnh::data::String<6> name;
    std::uint16_t numbers[3] = {10, 20, 30};
};
NNH_SAVE_TYPE(Old, "view", NNH_SAVE_FIELD(coins, 1), NNH_SAVE_FIELD(pair, 4), NNH_SAVE_FIELD(items, 9),
              NNH_SAVE_FIELD(name, 10), NNH_SAVE_FIELD(numbers, 11));

/// What a newer declaration would see: field 1 retired, 4 and 11 gone, 9 kept, 10 became a u8 (dropped).
struct New {
    nnh::data::List<Item, 8> items;
    std::uint8_t name = 0;
};
NNH_SAVE_TYPE(New, "view", NNH_SAVE_FIELD(items, 9), NNH_SAVE_FIELD(name, 10), NNH_SAVE_RETIRED(1));

Bytes old_file() {
    Old o;
    for (int i = 0; i < 5; ++i) {
        Item it;
        it.kind = static_cast<std::uint16_t>(100 + i);
        it.label.assign(i == 3 ? "third" : "x");
        o.items.push_back(it);
    }
    o.name.assign("Zed");
    return encoded(o);
}

} // namespace view

namespace {

/// The view over a file's root fields (a step gets the same).
struct View {
    Bytes bytes = view::old_file();
    nnh::save::OldFields old() const {
        const std::size_t header = 16 + 4; // "view"
        return nnh::save::OldFields(bytes.data() + header, static_cast<std::uint32_t>(bytes.size() - header));
    }
};

using nnh::save::path;

} // namespace

NNH_TEST(save_old_fields_answer_has_kind_shape_and_count) {
    const View v;
    const nnh::save::OldFields old = v.old();
    CHECK(old.has(path(1)) && !old.has(path(2)) && old.has(path(4, 2)) && !old.has(path(4, 3)));
    CHECK(old.kind(path(1)) == nnh::save::WireKind::U32 && old.kind(path(2)) == nnh::save::WireKind::None);
    CHECK(old.kind(path(9)) == nnh::save::WireKind::Struct && old.shape(path(9)) == nnh::save::WireShape::List);
    CHECK(old.count(path(9)) == 5 && old.count(path(11)) == 3 && old.count(path(1)) == -1 && old.count(path(2)) == -1);
    CHECK(old.kind(path(9).item(3)) == nnh::save::WireKind::Struct && !old.has(path(9).item(5)));
    CHECK(old.shape(path(11).item(0)) == nnh::save::WireShape::One);
}

NNH_TEST(save_old_fields_read_retired_unknown_and_dropped_values) {
    const View v;
    const nnh::save::OldFields old = v.old();
    std::uint32_t coins = 0; // retired in New
    CHECK(old.get(path(1), coins) && coins == 300);
    nnh::data::String<6> name; // dropped in New (it became a u8)
    CHECK(old.get(path(10), name) && name == "Zed");
    std::uint16_t numbers[3] = {}; // unknown in New
    CHECK(old.get(path(11), numbers) && numbers[2] == 30);
}

NNH_TEST(save_old_fields_follow_nested_paths_and_items) {
    const View v;
    const nnh::save::OldFields old = v.old();
    std::int16_t b = 0;
    CHECK(old.get(path(4, 2), b) && b == 5);
    nnh::data::String<6> label;
    CHECK(old.get(path(9).item(3).field(2), label) && label == "third");
    std::uint32_t kind = 0;
    CHECK(old.get(path(9).item(4).field(1), kind) && kind == 104);
    std::uint16_t second = 0;
    CHECK(old.get(path(11).item(1), second) && second == 20);
}

NNH_TEST(save_old_fields_decode_a_region_into_a_save_struct) {
    const View v;
    const nnh::save::OldFields old = v.old();
    Pair pair;
    CHECK(old.get(path(4), pair) && pair.a == 4 && pair.b == 5);
    view::Item item;
    CHECK(old.get(path(9).item(2), item) && item.kind == 102 && item.label == "x");
    nnh::data::List<view::Item, 8> items;
    CHECK(old.get(path(9), items) && items.size() == 5 && items[3].label == "third");
}

NNH_TEST(save_old_fields_answer_false_when_the_kind_cannot_hold_the_value) {
    const View v;
    const nnh::save::OldFields old = v.old();
    std::uint8_t small = 7;
    CHECK(!old.get(path(1), small) && small == 7); // 300 doesn't fit a u8: unchanged
    std::uint32_t number = 9;
    CHECK(!old.get(path(10), number) && number == 9); // a string
    CHECK(!old.get(path(4), number) && number == 9);  // a struct
    Pair pair{8, 8};
    CHECK(!old.get(path(1), pair) && pair.a == 8);    // a number into a struct
    CHECK(!old.get(path(2), number) && number == 9);  // absent
    CHECK(!old.get(nnh::save::FieldPath{}, number));  // an empty path
    std::uint16_t one = 0;
    CHECK(!old.get(path(11), one)); // a sequence into one value
}

NNH_TEST(save_old_fields_keep_the_old_struct_pattern) {
    // A file of version 1 loads into the old struct's declaration (same key, version 1) as it was written.
    mig::V1 v1;
    v1.coins = 5;
    v1.weapon = 6;
    const Bytes b = encoded(v1);
    mig::V1 again;
    CHECK(nnh::save::decode(b.data(), b.size(), again).ok() && again.coins == 5 && again.weapon == 6);
    // and a newer file doesn't load into it
    mig::V3 v3;
    const Bytes newer = encoded(v3);
    CHECK(nnh::save::decode(newer.data(), newer.size(), again).error().code == nnh::ErrorCode::Unsupported);
}
