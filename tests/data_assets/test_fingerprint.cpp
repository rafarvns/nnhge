// The schema fingerprint (spec 014 FR-10, D29): a CRC-32 of a canonical encoding of a type and everything it
// reaches — key, field names in order, kinds, shapes, counts, capacities, integer widths and signs, effective
// ranges, `required`, enum names, nested types, referenced types' keys. A converted asset carries it, so an
// asset converted for an older struct is refused. It must not depend on anything the data doesn't: member
// order, default values, offsets, the C++ type name, the source file.

#include <cstdint>
#include <cstring>

#include "data/schema_fingerprint.hpp"
#include "data/schema_registry.hpp"
#include "test.hpp"
#include "test_types.hpp"

using nnh::detail::DataRegistry;
using nnh::detail::EnumInfo;
using nnh::detail::FieldInfo;
using nnh::detail::FieldKind;
using nnh::detail::make_type;
using nnh::detail::schema_fingerprint;
using nnh::detail::TypeInfo;

namespace {

enum class Mood : std::uint8_t { Calm, Angry };
struct Loot {
    int gold = 0;
};
struct Stats {
    std::int16_t hp = 5;
};
struct Mob {
    std::int16_t power = 3;
    nnh::data::String<8> name;
    Mood mood = Mood::Calm;
    Stats stats;
    nnh::data::Ref<Loot> loot;
    float speed = 1;
    nnh::data::List<float, 3> chances;
    bool flags[2] = {};
};
// The same fields, members in another order and other defaults.
struct MobReordered {
    bool flags[2] = {true, true};
    nnh::data::List<float, 3> chances;
    float speed = 9;
    nnh::data::Ref<Loot> loot;
    Stats stats;
    Mood mood = Mood::Angry;
    nnh::data::String<8> name;
    std::int16_t power = 100;
};

#define NNH_TEST_FIELDS(Name, T, ...)                                                                        \
    struct Name {                                                                                            \
        using Self = T;                                                                                      \
        static constexpr FieldInfo list[] = {__VA_ARGS__};                                                   \
    }
#define NNH_TEST_MOB_FIELDS                                                                                  \
    NNH_DATA_FIELD(power, .min = 1, .max = 50), NNH_DATA_FIELD(name, .required = true),                      \
        NNH_DATA_FIELD(mood), NNH_DATA_FIELD(stats), NNH_DATA_FIELD(loot), NNH_DATA_FIELD(speed),            \
        NNH_DATA_FIELD(chances), NNH_DATA_FIELD(flags)
NNH_TEST_FIELDS(MobFields, Mob, NNH_TEST_MOB_FIELDS);
NNH_TEST_FIELDS(MobReorderedFields, MobReordered, NNH_TEST_MOB_FIELDS);
NNH_TEST_FIELDS(StatsFields, Stats, NNH_DATA_FIELD(hp));
NNH_TEST_FIELDS(LootFields, Loot, NNH_DATA_FIELD(gold));

constexpr nnh::detail::DataEnumValues<2> kMoodValues{{0, 1}};
constexpr EnumInfo kMood = nnh::detail::make_enum<Mood>("Mood", "Calm, Angry", kMoodValues, "mood.cpp");
constexpr TypeInfo kMob = make_type<Mob>("Mob", "mob", "mob.cpp", MobFields::list);
constexpr TypeInfo kStats = make_type<Stats>("Stats", "", "mob.cpp", StatsFields::list);
constexpr TypeInfo kLoot = make_type<Loot>("Loot", "loot", "mob.cpp", LootFields::list);
constexpr int kMobFields = 8;

/// A local registry with Mob (or a changed copy of it), Stats, Loot and Mood, each also changeable.
struct Schema {
    FieldInfo mob_fields[kMobFields];
    FieldInfo stats_fields[1];
    TypeInfo mob = kMob;
    TypeInfo stats = kStats;
    TypeInfo loot = kLoot;
    EnumInfo mood = kMood;
    nnh::detail::TypeNode nodes[3];
    nnh::detail::EnumNode enum_node;
    DataRegistry registry;

    Schema() {
        std::memcpy(mob_fields, MobFields::list, sizeof mob_fields);
        std::memcpy(stats_fields, StatsFields::list, sizeof stats_fields);
    }
    std::uint32_t fingerprint() {
        mob.fields = mob_fields;
        stats.fields = stats_fields;
        registry = {};
        nodes[0].info = &mob;
        nodes[1].info = &stats;
        nodes[2].info = &loot;
        for (auto& n : nodes) add_type(registry, n);
        enum_node.info = &mood;
        add_enum(registry, enum_node);
        return schema_fingerprint(registry, mob);
    }
};

std::uint32_t base() { return Schema{}.fingerprint(); }

} // namespace

NNH_TEST(data_fingerprint_is_stable) {
    // Pinned: a change here means every converted data asset of every game is refused until rebuilt. Change
    // the encoding only on purpose (and say so in the plan).
    const nnh::detail::TypeInfo* enemy =
        find_type(nnh::detail::data_registry(), nnh::detail::type_id<test_data::Enemy>());
    CHECK(schema_fingerprint(nnh::detail::data_registry(), *enemy) == 0xFCEF3C32u);
    CHECK(base() == Schema{}.fingerprint());
}

NNH_TEST(data_fingerprint_ignores_member_order_and_defaults) {
    Schema s;
    std::memcpy(s.mob_fields, MobReorderedFields::list, sizeof s.mob_fields);
    s.mob.size = sizeof(MobReordered);
    CHECK(s.fingerprint() == base());
}

NNH_TEST(data_fingerprint_ignores_names_of_types_and_sources) {
    Schema s;
    s.mob.name = "Monster";
    s.mob.source = "elsewhere.cpp";
    s.stats.name = "Numbers";
    s.mood.name = "Feeling";
    s.loot.name = "Treasure";
    CHECK(s.fingerprint() == base());
}

NNH_TEST(data_fingerprint_ignores_the_order_of_the_registry) {
    Schema s;
    s.fingerprint();
    // Same nodes, linked the other way round.
    DataRegistry reversed;
    for (int i = 2; i >= 0; --i) add_type(reversed, s.nodes[i]);
    add_enum(reversed, s.enum_node);
    CHECK(schema_fingerprint(reversed, s.mob) == base());
}

NNH_TEST(data_fingerprint_changes_with_the_key) {
    Schema s;
    s.mob.key = "mobs";
    CHECK(s.fingerprint() != base());
}

NNH_TEST(data_fingerprint_changes_with_a_field_name_or_the_field_order) {
    Schema a;
    a.mob_fields[0].name = "powr";
    CHECK(a.fingerprint() != base());
    Schema b;
    const FieldInfo first = b.mob_fields[0];
    b.mob_fields[0] = b.mob_fields[1];
    b.mob_fields[1] = first;
    CHECK(b.fingerprint() != base());
}

NNH_TEST(data_fingerprint_changes_with_kind_shape_count_and_capacity) {
    Schema kind;
    kind.mob_fields[5].kind = FieldKind::Int; // speed
    CHECK(kind.fingerprint() != base());
    Schema shape;
    shape.mob_fields[6].shape = nnh::detail::FieldShape::Array; // chances
    CHECK(shape.fingerprint() != base());
    Schema count;
    count.mob_fields[6].count = 4;
    CHECK(count.fingerprint() != base());
    Schema capacity;
    capacity.mob_fields[1].capacity = 9; // name
    CHECK(capacity.fingerprint() != base());
}

NNH_TEST(data_fingerprint_changes_with_integer_width_and_sign) {
    Schema width;
    width.mob_fields[0].width = 4;
    CHECK(width.fingerprint() != base());
    Schema sign;
    sign.mob_fields[0].is_signed = false;
    CHECK(sign.fingerprint() != base());
}

NNH_TEST(data_fingerprint_changes_with_the_range_and_required) {
    Schema min;
    min.mob_fields[0].min = 2;
    CHECK(min.fingerprint() != base());
    Schema max;
    max.mob_fields[5].max = 10; // a float's range too
    CHECK(max.fingerprint() != base());
    Schema required;
    required.mob_fields[1].required = false;
    CHECK(required.fingerprint() != base());
}

NNH_TEST(data_fingerprint_changes_with_enum_names) {
    Schema renamed;
    renamed.mood.names = "Calm, Furious";
    CHECK(renamed.fingerprint() != base());
    Schema reordered;
    reordered.mood.names = "Angry, Calm";
    CHECK(reordered.fingerprint() != base());
}

NNH_TEST(data_fingerprint_ignores_enum_values_and_width) {
    // The file stores a name's index; the game's value table maps it.
    static constexpr std::int64_t other_values[] = {7, 9};
    Schema s;
    s.mood.values = other_values;
    s.mood.width = 4;
    s.mob_fields[2].width = 4;
    CHECK(s.fingerprint() == base());
}

NNH_TEST(data_fingerprint_changes_with_a_nested_struct) {
    Schema s;
    s.stats_fields[0].max = 10;
    CHECK(s.fingerprint() != base());
}

NNH_TEST(data_fingerprint_changes_with_the_referenced_key) {
    Schema s;
    s.loot.key = "treasure";
    CHECK(s.fingerprint() != base());
}

NNH_TEST(data_fingerprint_terminates_on_unknown_and_cyclic_types) {
    Schema unknown;
    unknown.mob_fields[3].target = &unknown; // a nested type nobody declared
    CHECK(unknown.fingerprint() != base());
    // A type nested in itself (only a damaged description can say that): still finishes.
    Schema cyclic;
    cyclic.stats_fields[0].kind = FieldKind::Struct;
    cyclic.stats_fields[0].target = kStats.tag;
    (void)cyclic.fingerprint();
    CHECK(true);
}
