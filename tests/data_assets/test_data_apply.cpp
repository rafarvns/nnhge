// Applying a validated converted data asset to a struct (spec 014 FR-2, FR-10, FR-11): values go through the
// compiled offsets — integers at the member's width, strings with the rest of their buffer zeroed, enums
// through the game's value table, list counts into the List — and unset fields keep what was there (the
// loader resets the struct to T{} first).

#include <cstdint>
#include <cstring>
#include <limits>

#include "data/data_apply.hpp"
#include "data/data_validate.hpp"
#include "data/schema_fingerprint.hpp"
#include "data/schema_registry.hpp"
#include "data_file.hpp"
#include "test.hpp"
#include "test_types.hpp"

using data_file::Bytes;
using data_file::Writer;
using nnh::detail::data_registry;
using nnh::detail::DataRegistry;
using nnh::detail::FieldInfo;
using nnh::detail::TypeInfo;
using test_data::Element;
using test_data::Enemy;

namespace {

const TypeInfo& enemy_type() { return *find_type(data_registry(), nnh::detail::type_id<Enemy>()); }

/// Validates (the precondition of apply) and applies `b` to `out`.
template <class T> void apply(const Bytes& b, const DataRegistry& r, const TypeInfo& t, T& out) {
    const std::uint32_t fp = nnh::detail::schema_fingerprint(r, t);
    CHECK(nnh::detail::validate_data(b.data(), b.size(), r, t, fp).ok());
    nnh::detail::apply_data(b.data(), b.size(), r, t, &out);
}

Writer enemy_file() { return Writer(nnh::detail::schema_fingerprint(data_registry(), enemy_type())); }

Enemy fresh() {
    Enemy e;
    enemy_type().reset(&e);
    return e;
}

// Every integer width at its extremes, a signed enum, a nested struct and an array of structs.
enum class Tilt : std::int16_t { Left = -300, Middle = 0, Right = 300 };
struct Pair {
    std::uint16_t a = 7;
    std::int8_t b = 8;
};
struct Numbers {
    std::int8_t i8 = 0;
    std::uint8_t u8 = 0;
    std::int16_t i16 = 0;
    std::uint16_t u16 = 0;
    std::int32_t i32 = 0;
    std::uint32_t u32 = 0;
    Tilt tilt = Tilt::Middle;
    Pair pairs[2];
    Pair one;
};
struct PairFields {
    using Self = Pair;
    static constexpr FieldInfo list[] = {NNH_DATA_FIELD(a), NNH_DATA_FIELD(b)};
};
struct NumbersFields {
    using Self = Numbers;
    static constexpr FieldInfo list[] = {NNH_DATA_FIELD(i8),   NNH_DATA_FIELD(u8),    NNH_DATA_FIELD(i16),
                                         NNH_DATA_FIELD(u16),  NNH_DATA_FIELD(i32),   NNH_DATA_FIELD(u32),
                                         NNH_DATA_FIELD(tilt), NNH_DATA_FIELD(pairs), NNH_DATA_FIELD(one)};
};
constexpr nnh::detail::DataEnumValues<3> kTiltValues{{-300, 0, 300}};
constexpr nnh::detail::EnumInfo kTilt =
    nnh::detail::make_enum<Tilt>("Tilt", "Left, Middle, Right", kTiltValues, "t");
constexpr TypeInfo kPair = nnh::detail::make_type<Pair>("Pair", "", "t", PairFields::list);
constexpr TypeInfo kNumbers = nnh::detail::make_type<Numbers>("Numbers", "numbers", "t", NumbersFields::list);

struct NumbersSchema {
    DataRegistry registry;
    nnh::detail::TypeNode nodes[2] = {{&kPair, nullptr}, {&kNumbers, nullptr}};
    nnh::detail::EnumNode tilt{&kTilt, nullptr};
    NumbersSchema() {
        add_type(registry, nodes[0]);
        add_type(registry, nodes[1]);
        add_enum(registry, tilt);
    }
};

} // namespace

NNH_TEST(data_apply_sets_every_kind) {
    Writer w = enemy_file();
    w.fields(12).field(0).str("Goblin");
    w.field(1).fields(2).field(0).u16(30).field(1).u8(0xFB);
    w.field(2).f32(1.25f).field(3).u8(1).field(4).u32(0x11111111u).field(5).u32(0x22222222u);
    w.field(6).u16(2);
    w.fields(2).field(0).u32(0x33333333u).field(1).f32(0.25f);
    w.fields(1).field(0).u32(0x44444444u);
    w.field(7).u8(10).u8(20).u8(30).u8(40).field(8).f32(3).f32(-4);
    w.field(9).u8(1).u8(0).u8(1).field(10).u8(42).field(11).u32(0x55555555u);
    Enemy e = fresh();
    apply(w.bytes(), data_registry(), enemy_type(), e);
    CHECK(e.name == "Goblin");
    CHECK(e.stats.hp == 30 && e.stats.attack == -5);
    CHECK(e.speed == 1.25f);
    CHECK(e.element == Element::Fire); // name index 1 → the value 5
    CHECK(e.sprite.id == 0x11111111u && e.hurt.id == 0x22222222u && e.theme.id == 0x55555555u);
    CHECK(e.drops.size() == 2);
    CHECK(e.drops[0].item.id == 0x33333333u && e.drops[0].chance == 0.25f);
    CHECK(e.drops[1].item.id == 0x44444444u && e.drops[1].chance == 0.5f); // unset in the item: its default
    CHECK(e.tint.r == 10 && e.tint.g == 20 && e.tint.b == 30 && e.tint.a == 40);
    CHECK(e.spawn.x == 3 && e.spawn.y == -4);
    CHECK(e.flags[0] && !e.flags[1] && e.flags[2]);
    CHECK(e.level == 42);
}

NNH_TEST(data_apply_keeps_defaults_of_unset_fields) {
    Enemy e = fresh();
    apply(enemy_file().fields(1).field(0).str("Imp").bytes(), data_registry(), enemy_type(), e);
    const Enemy defaults{};
    CHECK(e.name == "Imp");
    CHECK(e.stats.hp == defaults.stats.hp && e.speed == defaults.speed && e.element == defaults.element);
    CHECK(e.drops.empty() && e.drops.begin()[3].chance == 0.5f);
    CHECK(e.tint.a == 255 && e.spawn.y == 16 && e.flags[1] && e.level == 1 && e.theme.empty());
}

NNH_TEST(data_apply_zeroes_the_rest_of_a_string) {
    Enemy e = fresh();
    CHECK(e.name.assign("abcdefghijklmnopqrstuvwx"));
    apply(enemy_file().fields(1).field(0).str("Imp").bytes(), data_registry(), enemy_type(), e);
    nnh::data::String<24> expected;
    CHECK(expected.assign("Imp"));
    CHECK(std::memcmp(&e.name, &expected, sizeof expected) == 0);
}

NNH_TEST(data_apply_leaves_unwritten_list_slots_alone) {
    Enemy e = fresh();
    Writer w = enemy_file();
    w.fields(2).field(0).str("Imp").field(6).u16(1).fields(2).field(0).u32(9).field(1).f32(1);
    apply(w.bytes(), data_registry(), enemy_type(), e);
    CHECK(e.drops.size() == 1 && e.drops[0].item.id == 9u && e.drops[0].chance == 1.0f);
    CHECK(e.drops.begin()[1].item.empty() && e.drops.begin()[1].chance == 0.5f);
}

NNH_TEST(data_apply_writes_integers_at_their_width) {
    NumbersSchema s;
    Writer w(nnh::detail::schema_fingerprint(s.registry, kNumbers));
    w.fields(6).field(0).u8(0x80).field(1).u8(0xFF).field(2).u16(0x8000).field(3).u16(0xFFFF);
    w.field(4).u32(0x80000000u).field(5).u32(0xFFFFFFFFu);
    Numbers n;
    kNumbers.reset(&n);
    apply(w.bytes(), s.registry, kNumbers, n);
    CHECK(n.i8 == -128 && n.u8 == 255 && n.i16 == -32768 && n.u16 == 65535);
    CHECK(n.i32 == std::numeric_limits<std::int32_t>::min() && n.u32 == 0xFFFFFFFFu);
    CHECK(n.tilt == Tilt::Middle && n.pairs[1].a == 7); // untouched neighbours
}

NNH_TEST(data_apply_maps_enum_names_to_the_game_values) {
    NumbersSchema s;
    Numbers n;
    kNumbers.reset(&n);
    apply(Writer(nnh::detail::schema_fingerprint(s.registry, kNumbers)).fields(1).field(6).u8(0).bytes(),
          s.registry, kNumbers, n);
    CHECK(n.tilt == Tilt::Left);
    apply(Writer(nnh::detail::schema_fingerprint(s.registry, kNumbers)).fields(1).field(6).u8(2).bytes(),
          s.registry, kNumbers, n);
    CHECK(n.tilt == Tilt::Right);
    CHECK(n.i16 == 0); // a 2-byte enum doesn't spill into its neighbour
}

NNH_TEST(data_apply_fills_arrays_of_structs_and_nested_structs) {
    NumbersSchema s;
    Writer w(nnh::detail::schema_fingerprint(s.registry, kNumbers));
    w.fields(2).field(7).fields(1).field(0).u16(100).fields(2).field(0).u16(200).field(1).u8(3);
    w.field(8).fields(1).field(1).u8(0xFF);
    Numbers n;
    kNumbers.reset(&n);
    apply(w.bytes(), s.registry, kNumbers, n);
    CHECK(n.pairs[0].a == 100 && n.pairs[0].b == 8);
    CHECK(n.pairs[1].a == 200 && n.pairs[1].b == 3);
    CHECK(n.one.a == 7 && n.one.b == -1);
}

NNH_TEST(data_apply_stays_in_bounds_on_unvalidated_input) {
    // Apply needs a validated file; given anything else it must still not write outside the struct.
    struct Guarded {
        Enemy enemy;
        unsigned char guard[64];
    } g;
    enemy_type().reset(&g.enemy);
    std::memset(g.guard, 0x5A, sizeof g.guard);
    Writer w = enemy_file();
    w.fields(3).field(0).u8(200).raw("xxxxxxxx", 8).field(6).u16(60000).field(3).u8(250);
    const Bytes b = w.bytes();
    nnh::detail::apply_data(b.data(), b.size(), data_registry(), enemy_type(), &g.enemy);
    bool intact = true;
    for (unsigned char c : g.guard) intact = intact && c == 0x5A;
    CHECK(intact);
    CHECK(g.enemy.drops.size() <= 4);
}

NNH_TEST(data_apply_ignores_an_enum_index_past_the_names) {
    // Unvalidated input: index 3 of a 3-name enum must not read the value table past its end (here a sentinel).
    static constexpr std::int64_t values_and_sentinel[] = {-300, 0, 300, 123};
    nnh::detail::EnumInfo tilt = kTilt;
    tilt.values = values_and_sentinel;
    DataRegistry r;
    nnh::detail::TypeNode nodes[2] = {{&kPair, nullptr}, {&kNumbers, nullptr}};
    nnh::detail::EnumNode tilt_node{&tilt, nullptr};
    add_type(r, nodes[0]);
    add_type(r, nodes[1]);
    add_enum(r, tilt_node);
    const Bytes b = Writer(0).fields(1).field(6).u8(3).bytes();
    Numbers n;
    kNumbers.reset(&n);
    nnh::detail::apply_data(b.data(), b.size(), r, kNumbers, &n);
    CHECK(n.tilt == Tilt::Middle);
}

NNH_TEST(data_apply_ignores_a_buffer_shorter_than_a_header) {
    // A body that would set i8 = 5 sits after the 19 bytes given: nothing past `size` is read.
    NumbersSchema s;
    const Bytes b = Writer(0).fields(1).field(0).u8(5).bytes();
    Numbers n;
    kNumbers.reset(&n);
    nnh::detail::apply_data(b.data(), 19, s.registry, kNumbers, &n);
    CHECK(n.i8 == 0);
}
