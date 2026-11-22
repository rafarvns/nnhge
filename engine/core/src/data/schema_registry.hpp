#pragma once
// The registry of data types and enums (spec 014, D29): a list of the nodes NNH_DATA_TYPE / NNH_DATA_STRUCT /
// NNH_DATA_ENUM link in before main(), and lookups by type id, key and name. Pure (only <nnh/data.hpp>'s
// tables): also compiled into each game's schema host program and into nnh-assetc, which fills a local
// registry from a schema description. Load time only: lookups walk the list (at most 128 types and 64 enums).
// Tests: tests/data_assets/test_schema_registry.cpp.

#include "nnh/data.hpp"

namespace nnh::detail {

struct DataRegistry {
    TypeNode* types = nullptr;
    EnumNode* enums = nullptr;
};

/// The program's registry (what the macros registered). constinit: safe to use from static constructors.
DataRegistry& data_registry();

void add_type(DataRegistry& registry, TypeNode& node);
void add_enum(DataRegistry& registry, EnumNode& node);

/// nullptr when absent. find_type_by_key never finds keyless structs (key "").
const TypeInfo* find_type(const DataRegistry& registry, const void* tag);
const TypeInfo* find_type_by_key(const DataRegistry& registry, const char* key);
const TypeInfo* find_type_by_name(const DataRegistry& registry, const char* name);
const EnumInfo* find_enum(const DataRegistry& registry, const void* tag);
const EnumInfo* find_enum_by_name(const DataRegistry& registry, const char* name);

int count_types(const DataRegistry& registry);
int count_enums(const DataRegistry& registry);

} // namespace nnh::detail
