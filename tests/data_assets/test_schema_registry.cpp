// The registry of data types (spec 014 FR-1): NNH_DATA_TYPE / STRUCT / ENUM link static nodes into the
// program's registry before main(); lookups by type id, key and name. The tests' schema (test_types.cpp) is
// registered in nnh_tests; local registries show the lookups on their own.

#include <cstring>

#include "data/schema_registry.hpp"
#include "test.hpp"
#include "test_types.hpp"

using nnh::detail::data_registry;
using nnh::detail::DataRegistry;
using nnh::detail::type_id;

namespace {

bool ends_with(const char* text, const char* end) {
    const std::size_t n = std::strlen(text);
    const std::size_t m = std::strlen(end);
    return n >= m && std::strcmp(text + n - m, end) == 0;
}

struct Local {
    int a = 0;
};
struct LocalFields {
    using Self = Local;
    static constexpr nnh::detail::FieldInfo list[] = {NNH_DATA_FIELD(a)};
};
constexpr nnh::detail::TypeInfo kLocal =
    nnh::detail::make_type<Local>("Local", "local", "x.cpp", LocalFields::list);

} // namespace

NNH_TEST(data_registry_holds_the_types_declared_by_macros) {
    const nnh::detail::TypeInfo* enemy = find_type(data_registry(), type_id<test_data::Enemy>());
    CHECK(enemy != nullptr);
    CHECK(std::strcmp(enemy->name, "Enemy") == 0);
    CHECK(std::strcmp(enemy->key, "enemy") == 0);
    CHECK(ends_with(enemy->source, "test_types.cpp"));
    CHECK(enemy->field_count == 12);
    CHECK(enemy->size == sizeof(test_data::Enemy));
}

NNH_TEST(data_registry_keeps_structs_without_a_key) {
    const nnh::detail::TypeInfo* stats = find_type(data_registry(), type_id<test_data::Stats>());
    CHECK(stats != nullptr);
    CHECK(std::strcmp(stats->key, "") == 0);
    CHECK(stats->field_count == 2);
}

NNH_TEST(data_registry_finds_types_by_key_and_name) {
    CHECK(find_type_by_key(data_registry(), "item") ==
          find_type(data_registry(), type_id<test_data::Item>()));
    CHECK(find_type_by_key(data_registry(), "nope") == nullptr);
    CHECK(find_type_by_key(data_registry(), "") == nullptr); // keyless structs are never found by key
    CHECK(find_type_by_name(data_registry(), "Drop") ==
          find_type(data_registry(), type_id<test_data::Drop>()));
    CHECK(find_type_by_name(data_registry(), "drop") == nullptr);
}

NNH_TEST(data_registry_holds_enums_with_their_values) {
    const nnh::detail::EnumInfo* e = find_enum(data_registry(), type_id<test_data::Element>());
    CHECK(e != nullptr);
    CHECK(std::strcmp(e->name, "Element") == 0);
    CHECK(std::strcmp(e->names, "Normal, Fire, Ice") == 0);
    CHECK(e->count == 3);
    CHECK(e->values[0] == 0 && e->values[1] == 5 && e->values[2] == 6);
    CHECK(e->width == 1 && !e->is_signed);
    CHECK(find_enum_by_name(data_registry(), "Element") == e);
    CHECK(find_enum(data_registry(), type_id<test_data::Stats>()) == nullptr);
}

NNH_TEST(data_registry_counts_what_it_holds) {
    CHECK(count_types(data_registry()) == 4);
    CHECK(count_enums(data_registry()) == 1);
}

NNH_TEST(data_local_registry_starts_empty) {
    const DataRegistry r;
    CHECK(find_type(r, type_id<Local>()) == nullptr);
    CHECK(find_type_by_key(r, "local") == nullptr);
    CHECK(count_types(r) == 0 && count_enums(r) == 0);
}

NNH_TEST(data_local_registry_finds_what_was_added) {
    DataRegistry r;
    nnh::detail::TypeNode node{&kLocal, nullptr};
    add_type(r, node);
    CHECK(find_type(r, type_id<Local>()) == &kLocal);
    CHECK(find_type_by_key(r, "local") == &kLocal);
    CHECK(find_type_by_name(r, "Local") == &kLocal);
    CHECK(count_types(r) == 1);
    CHECK(find_type(data_registry(), type_id<Local>()) == nullptr); // the program's registry is untouched
}
