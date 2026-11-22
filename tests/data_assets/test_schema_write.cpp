// The schema description (spec 014 FR-3, D29): the text the schema host program writes and nnh-assetc reads —
// a version line, then enums and types sorted by name, one line per enum, type or field. Never offsets or
// sizes, so it doesn't depend on any compiler's layout. The exact text of the tests' schema is pinned here.

#include <cstdio>
#include <string>

#include "data/schema_fingerprint.hpp"
#include "data/schema_registry.hpp"
#include "data/schema_write.hpp"
#include "test.hpp"
#include "test_types.hpp"

using nnh::detail::data_registry;
using nnh::detail::DataRegistry;
using nnh::detail::FieldInfo;
using nnh::detail::make_type;
using nnh::detail::type_id;
using nnh::detail::TypeInfo;

namespace {

void append(void* context, const char* text, std::size_t size) {
    static_cast<std::string*>(context)->append(text, size);
}

std::string describe(const DataRegistry& r) {
    std::string out;
    nnh::detail::write_schema(r, append, &out);
    return out;
}

/// "type <name> <key> <fields> <fingerprint> <source>" for a type of the tests' schema.
template <class T> std::string type_line(const char* name, const char* key, int fields) {
    const TypeInfo* t = find_type(data_registry(), type_id<T>());
    char line[512];
    std::snprintf(line, sizeof line, "type %s %s %d %08X %s\n", name, key, fields,
                  static_cast<unsigned>(nnh::detail::schema_fingerprint(data_registry(), *t)), t->source);
    return line;
}

struct Ranges {
    std::int32_t full = 0;
    std::int16_t low = 0;
    std::uint32_t high = 0;
    float ratio = 0;
    float any = 0;
    std::uint16_t whole = 0;
};
struct RangesFields {
    using Self = Ranges;
    static constexpr FieldInfo list[] = {NNH_DATA_FIELD(full, .min = -2147483648.0, .max = 2147483647.0),
                                         NNH_DATA_FIELD(low, .min = -100),
                                         NNH_DATA_FIELD(high, .max = 4000000000.0),
                                         NNH_DATA_FIELD(ratio, .min = -0.5, .max = 0.1), NNH_DATA_FIELD(any),
                                         NNH_DATA_FIELD(whole)};
};
constexpr TypeInfo kRanges = make_type<Ranges>("Ranges", "ranges", "r.cpp", RangesFields::list);

} // namespace

NNH_TEST(data_schema_description_of_the_tests_schema) {
    const std::string expected = "nnh-data-schema 1\n"
                                 "enum Element 3 Normal Fire Ice\n" +
                                 type_line<test_data::Drop>("Drop", "-", 2) +
                                 "field item one ref Item required\n"
                                 "field chance one float min 0 max 1\n" +
                                 type_line<test_data::Enemy>("Enemy", "enemy", 12) +
                                 "field name one string 24 required\n"
                                 "field stats one struct Stats\n"
                                 "field speed one float min 0\n"
                                 "field element one enum Element\n"
                                 "field sprite one texture\n"
                                 "field hurt one sound\n"
                                 "field drops list 4 struct Drop\n"
                                 "field tint one color\n"
                                 "field spawn one vec2\n"
                                 "field flags array 3 bool\n"
                                 "field level one u8 min 1 max 99\n"
                                 "field theme one music\n" +
                                 type_line<test_data::Item>("Item", "item", 3) +
                                 "field name one string 16\n"
                                 "field price one i32 min 0\n"
                                 "field icon one texture\n" +
                                 type_line<test_data::Stats>("Stats", "-", 2) +
                                 "field hp one u16 min 1 max 999 required\n"
                                 "field attack one i8\n";
    const std::string actual = describe(data_registry());
    CHECK(actual == expected);
    if (actual != expected) std::printf("%s\n---\n%s\n", actual.c_str(), expected.c_str());
}

NNH_TEST(data_schema_description_pins_the_enemy_fingerprint) {
    CHECK(describe(data_registry()).find("type Enemy enemy 12 FCEF3C32 ") != std::string::npos);
}

NNH_TEST(data_schema_description_writes_only_ranges_narrower_than_the_type) {
    DataRegistry r;
    nnh::detail::TypeNode node{&kRanges, nullptr};
    add_type(r, node);
    const std::string text = describe(r);
    CHECK(text.find("field full one i32\n") != std::string::npos);
    CHECK(text.find("field low one i16 min -100\n") != std::string::npos);
    CHECK(text.find("field high one u32 max 4000000000\n") != std::string::npos);
    CHECK(text.find("field ratio one float min -0.5 max 0.10000000000000001\n") != std::string::npos);
    CHECK(text.find("field any one float\n") != std::string::npos);
    CHECK(text.find("field whole one u16\n") != std::string::npos); // unsigned, full range: no bounds
}

NNH_TEST(data_schema_description_does_not_depend_on_registration_order) {
    DataRegistry reversed;
    nnh::detail::TypeNode types[8];
    nnh::detail::EnumNode enums[4];
    int t = 0;
    int e = 0;
    // Re-link the program's nodes' infos the other way round (the list is newest first).
    for (const nnh::detail::TypeNode* n = data_registry().types; n != nullptr; n = n->next)
        types[t++].info = n->info;
    for (const nnh::detail::EnumNode* n = data_registry().enums; n != nullptr; n = n->next)
        enums[e++].info = n->info;
    for (int i = 0; i < t; ++i) add_type(reversed, types[i]);
    for (int i = 0; i < e; ++i) add_enum(reversed, enums[i]);
    CHECK(describe(reversed) == describe(data_registry()));
}

NNH_TEST(data_schema_description_of_an_empty_schema_is_the_version_line) {
    CHECK(describe(DataRegistry{}) == "nnh-data-schema 1\n");
}
