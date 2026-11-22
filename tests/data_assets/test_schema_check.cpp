// Checking a whole schema (spec 014 FR-1, FR-14): what one NNH_DATA_TYPE can't see on its own — two types
// with one key, a nested struct or enum that was never declared, a Ref to a keyless type, nesting deeper than
// 4, reserved or malformed keys, too many types. The schema host program runs it before writing the
// description; each problem is
// "<schema source>: <reason>". Local registries here; the tests' schema is checked last.

#include <cstdint>
#include <string>
#include <vector>

#include "data/schema_check.hpp"
#include "data/schema_registry.hpp"
#include "test.hpp"

using nnh::detail::DataRegistry;
using nnh::detail::EnumInfo;
using nnh::detail::FieldInfo;
using nnh::detail::make_type;
using nnh::detail::TypeInfo;

namespace {

struct Issues {
    std::vector<std::string> list;
    bool has(const char* fragment) const {
        for (const std::string& s : list) {
            if (s.find(fragment) != std::string::npos) return true;
        }
        return false;
    }
};

void collect(void* context, const char* message) {
    static_cast<Issues*>(context)->list.emplace_back(message);
}

/// A registry holding the given types and enums (nodes owned here), and its check.
struct Reg {
    DataRegistry registry;
    nnh::detail::TypeNode nodes[160];
    nnh::detail::EnumNode enum_nodes[80];
    int used = 0;
    int enums_used = 0;
    Reg& add(const TypeInfo& t) {
        nodes[used].info = &t;
        add_type(registry, nodes[used++]);
        return *this;
    }
    Reg& add(const EnumInfo& e) {
        enum_nodes[enums_used].info = &e;
        add_enum(registry, enum_nodes[enums_used++]);
        return *this;
    }
    Issues check() const {
        Issues issues;
        const int n = nnh::detail::check_schema(registry, collect, &issues);
        CHECK(n == static_cast<int>(issues.list.size()));
        return issues;
    }
};

enum class Color3 : std::uint8_t { Red, Green, Blue };
struct Inner {
    int v = 0;
};
struct Item {
    int price = 0;
};
struct Outer {
    Inner inner;
    Color3 color = Color3::Red;
    nnh::data::Ref<Item> item;
    nnh::data::Ref<Inner> bad_ref;
};

#define NNH_TEST_FIELDS(Name, T, ...)                                                                        \
    struct Name {                                                                                            \
        using Self = T;                                                                                      \
        static constexpr FieldInfo list[] = {__VA_ARGS__};                                                   \
    }
NNH_TEST_FIELDS(InnerFields, Inner, NNH_DATA_FIELD(v));
NNH_TEST_FIELDS(ItemFields, Item, NNH_DATA_FIELD(price));
NNH_TEST_FIELDS(OuterFields, Outer, NNH_DATA_FIELD(inner), NNH_DATA_FIELD(color), NNH_DATA_FIELD(item));
NNH_TEST_FIELDS(BadRefFields, Outer, NNH_DATA_FIELD(bad_ref));

constexpr nnh::detail::DataEnumValues<3> kColorValues{{0, 1, 2}};
constexpr EnumInfo kColor =
    nnh::detail::make_enum<Color3>("Color3", "Red, Green, Blue", kColorValues, "colors.cpp");
constexpr EnumInfo kColorTwice =
    nnh::detail::make_enum<Color3>("Color3", "Red, Green, Red", kColorValues, "other.cpp");
constexpr TypeInfo kInner = make_type<Inner>("Inner", "", "inner.cpp", InnerFields::list);
constexpr TypeInfo kItem = make_type<Item>("Item", "item", "item.cpp", ItemFields::list);
constexpr TypeInfo kOuter = make_type<Outer>("Outer", "outer", "outer.cpp", OuterFields::list);
constexpr TypeInfo kBadRef = make_type<Outer>("BadRef", "badref", "badref.cpp", BadRefFields::list);

/// Outer, Inner, Item and Color3: a complete schema.
Reg& complete(Reg& r) { return r.add(kInner).add(kItem).add(kOuter).add(kColor); }

// Nesting chain: L0 holds L1 ... holds L5, five struct levels below L0.
struct L5 {
    int v = 0;
};
struct L4 {
    L5 next;
};
struct L3 {
    L4 next;
};
struct L2 {
    L3 next;
};
struct L1 {
    L2 next;
};
struct L0 {
    L1 next;
};
NNH_TEST_FIELDS(L5Fields, L5, NNH_DATA_FIELD(v));
NNH_TEST_FIELDS(L4Fields, L4, NNH_DATA_FIELD(next));
NNH_TEST_FIELDS(L3Fields, L3, NNH_DATA_FIELD(next));
NNH_TEST_FIELDS(L2Fields, L2, NNH_DATA_FIELD(next));
NNH_TEST_FIELDS(L1Fields, L1, NNH_DATA_FIELD(next));
NNH_TEST_FIELDS(L0Fields, L0, NNH_DATA_FIELD(next));
constexpr TypeInfo kL5 = make_type<L5>("L5", "", "levels.cpp", L5Fields::list);
constexpr TypeInfo kL4 = make_type<L4>("L4", "", "levels.cpp", L4Fields::list);
constexpr TypeInfo kL3 = make_type<L3>("L3", "", "levels.cpp", L3Fields::list);
constexpr TypeInfo kL2 = make_type<L2>("L2", "", "levels.cpp", L2Fields::list);
constexpr TypeInfo kL1 = make_type<L1>("L1", "four", "levels.cpp", L1Fields::list);
constexpr TypeInfo kL0 = make_type<L0>("L0", "five", "levels.cpp", L0Fields::list);

TypeInfo with_key(const TypeInfo& t, const char* key) {
    TypeInfo copy = t;
    copy.key = key;
    return copy;
}

} // namespace

NNH_TEST(data_schema_check_accepts_a_complete_schema) {
    Reg r;
    const Issues issues = complete(r).check();
    CHECK(issues.list.empty());
}

NNH_TEST(data_schema_check_accepts_an_empty_schema) { CHECK(Reg{}.check().list.empty()); }

NNH_TEST(data_schema_check_accepts_the_tests_schema) {
    Issues issues;
    CHECK(nnh::detail::check_schema(nnh::detail::data_registry(), collect, &issues) == 0);
}

NNH_TEST(data_schema_check_finds_two_types_with_one_key) {
    static const char foe_type = 0;
    TypeInfo foe = with_key(kItem, "outer");
    foe.tag = &foe_type;
    foe.name = "Foe";
    foe.source = "foe.cpp";
    Reg r;
    complete(r);
    r.add(foe);
    const Issues issues = r.check();
    CHECK(issues.list.size() == 1);
    CHECK(issues.has("types Foe and Outer both use the key 'outer'"));
    CHECK(issues.has("foe.cpp") && issues.has("outer.cpp")); // names both sources
}

NNH_TEST(data_schema_check_finds_a_type_declared_twice) {
    Reg r;
    complete(r);
    r.add(kItem);
    const Issues issues = r.check();
    CHECK(issues.has("item.cpp: type Item is declared twice"));
}

NNH_TEST(data_schema_check_finds_two_types_with_one_name) {
    const TypeInfo renamed = [] {
        TypeInfo t = kBadRef;
        t.name = "Item";
        t.fields = ItemFields::list; // a valid field list: only the name clashes
        t.key = "other";
        static const char other_type = 0;
        t.tag = &other_type; // another C++ type
        return t;
    }();
    Reg r;
    complete(r);
    r.add(renamed);
    CHECK(r.check().has("two types are named 'Item'"));
}

NNH_TEST(data_schema_check_finds_a_nested_struct_never_declared) {
    Reg r;
    r.add(kItem).add(kOuter).add(kColor);
    CHECK(r.check().has("outer.cpp: Outer.inner: its type has no NNH_DATA_STRUCT or NNH_DATA_TYPE"));
}

NNH_TEST(data_schema_check_finds_an_enum_never_declared) {
    Reg r;
    r.add(kInner).add(kItem).add(kOuter);
    CHECK(r.check().has("outer.cpp: Outer.color: its enum has no NNH_DATA_ENUM"));
}

NNH_TEST(data_schema_check_finds_a_reference_to_an_undeclared_type) {
    Reg r;
    r.add(kInner).add(kOuter).add(kColor);
    CHECK(r.check().has("outer.cpp: Outer.item: Ref<T> needs a T declared with NNH_DATA_TYPE"));
}

NNH_TEST(data_schema_check_finds_a_reference_to_a_keyless_type) {
    Reg r;
    complete(r);
    r.add(kBadRef);
    CHECK(r.check().has(
        "badref.cpp: BadRef.bad_ref: Ref<Inner> needs a type with a key: Inner is an NNH_DATA_STRUCT"));
}

NNH_TEST(data_schema_check_allows_four_nested_levels) {
    Reg r;
    r.add(kL5).add(kL4).add(kL3).add(kL2).add(kL1);
    CHECK(r.check().list.empty());
}

NNH_TEST(data_schema_check_refuses_five_nested_levels) {
    Reg r;
    r.add(kL5).add(kL4).add(kL3).add(kL2).add(kL1).add(kL0);
    const Issues issues = r.check();
    CHECK(issues.list.size() == 1);
    CHECK(issues.has("levels.cpp: type L0: structs nested 5 deep, at most 4"));
}

NNH_TEST(data_schema_check_refuses_malformed_keys) {
    const char* bad[] = {"Enemy", "1st", "_x", "a-b", "a.b", "abcdefghijklmnopq"};
    for (const char* key : bad) {
        const TypeInfo t = with_key(kItem, key);
        Reg r;
        r.add(t);
        const std::string expected =
            "item.cpp: type Item: key '" + std::string(key) +
            "' must be 1 to 16 characters among a-z, 0-9 and _, starting with a letter";
        CHECK(r.check().has(expected.c_str()));
    }
}

NNH_TEST(data_schema_check_accepts_keys_at_the_limits) {
    const char* good[] = {"a", "abcdefghijklmnop", "a_9", "z0"};
    for (const char* key : good) {
        const TypeInfo t = with_key(kItem, key);
        Reg r;
        r.add(t);
        CHECK(r.check().list.empty());
    }
}

NNH_TEST(data_schema_check_refuses_reserved_keys) {
    const char* reserved[] = {"png", "wav", "ogg", "tmj", "tsj", "nnhmap", "nnh_data", "t3x"};
    for (const char* key : reserved) {
        const TypeInfo t = with_key(kItem, key);
        Reg r;
        r.add(t);
        const std::string expected = "item.cpp: type Item: key '" + std::string(key) + "' is reserved";
        CHECK(r.check().has(expected.c_str()));
    }
}

NNH_TEST(data_schema_check_finds_an_enum_declared_twice_and_a_repeated_name) {
    Reg r;
    complete(r);
    r.add(kColorTwice);
    const Issues issues = r.check();
    CHECK(issues.has("enum Color3 is declared twice"));
    CHECK(issues.has("other.cpp: enum Color3: the name 'Red' is listed twice"));
}

NNH_TEST(data_schema_check_refuses_more_than_64_data_types) {
    static TypeInfo many[65];
    static char keys[65][8];
    Reg r;
    for (int i = 0; i < 65; ++i) {
        keys[i][0] = 'k';
        keys[i][1] = static_cast<char>('a' + i / 26);
        keys[i][2] = static_cast<char>('a' + i % 26);
        keys[i][3] = '\0';
        many[i] = with_key(kItem, keys[i]);
        many[i].tag = &keys[i]; // distinct types
        many[i].name = keys[i];
    }
    for (int i = 0; i < 64; ++i) r.add(many[i]);
    CHECK(r.check().list.empty());
    r.add(many[64]);
    CHECK(r.check().has("the schema declares 65 data types: at most 64"));
}

NNH_TEST(data_schema_check_refuses_more_than_128_types) {
    static TypeInfo many[129];
    static char names[129][8];
    Reg r;
    for (int i = 0; i < 129; ++i) {
        names[i][0] = 'S';
        names[i][1] = static_cast<char>('a' + i / 26);
        names[i][2] = static_cast<char>('a' + i % 26);
        names[i][3] = '\0';
        many[i] = kInner;
        many[i].tag = &names[i];
        many[i].name = names[i];
    }
    for (int i = 0; i < 128; ++i) r.add(many[i]);
    CHECK(r.check().list.empty());
    r.add(many[128]);
    CHECK(r.check().has("the schema declares 129 types and structs: at most 128"));
}

NNH_TEST(data_schema_check_refuses_more_than_64_enums) {
    static EnumInfo many[65];
    static char names[65][8];
    Reg r;
    for (int i = 0; i < 65; ++i) {
        names[i][0] = 'E';
        names[i][1] = static_cast<char>('a' + i / 26);
        names[i][2] = static_cast<char>('a' + i % 26);
        names[i][3] = '\0';
        many[i] = kColor;
        many[i].tag = &names[i];
        many[i].name = names[i];
    }
    for (int i = 0; i < 64; ++i) r.add(many[i]);
    CHECK(r.check().list.empty());
    r.add(many[64]);
    CHECK(r.check().has("the schema declares 65 enums: at most 64"));
}

NNH_TEST(data_schema_check_refuses_names_over_63_characters) {
    static const char long_name[] = "a_name_that_is_far_too_long_for_a_schema_description_line_of_text";
    TypeInfo t = kItem;
    t.name = long_name;
    Reg r;
    r.add(t);
    CHECK(r.check().has("is longer than 63 characters"));
}
