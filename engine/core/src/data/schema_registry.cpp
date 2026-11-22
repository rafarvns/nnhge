// The registry of data types and enums (schema_registry.hpp). Pure: shared with the schema host program and
// nnh-assetc.

#include "data/schema_registry.hpp"

#include <cstring>

namespace nnh::detail {

namespace {
constinit DataRegistry g_registry{};
} // namespace

DataRegistry& data_registry() { return g_registry; }

TypeRegistrar::TypeRegistrar(TypeNode& node) { add_type(g_registry, node); }
EnumRegistrar::EnumRegistrar(EnumNode& node) { add_enum(g_registry, node); }

void add_type(DataRegistry& registry, TypeNode& node) {
    node.next = registry.types;
    registry.types = &node;
}

void add_enum(DataRegistry& registry, EnumNode& node) {
    node.next = registry.enums;
    registry.enums = &node;
}

const TypeInfo* find_type(const DataRegistry& registry, const void* tag) {
    for (const TypeNode* n = registry.types; n != nullptr; n = n->next) {
        if (n->info->tag == tag) return n->info;
    }
    return nullptr;
}

const TypeInfo* find_type_by_key(const DataRegistry& registry, const char* key) {
    if (key == nullptr || key[0] == '\0') return nullptr;
    for (const TypeNode* n = registry.types; n != nullptr; n = n->next) {
        if (std::strcmp(n->info->key, key) == 0) return n->info;
    }
    return nullptr;
}

const TypeInfo* find_type_by_name(const DataRegistry& registry, const char* name) {
    for (const TypeNode* n = registry.types; n != nullptr; n = n->next) {
        if (std::strcmp(n->info->name, name) == 0) return n->info;
    }
    return nullptr;
}

const EnumInfo* find_enum(const DataRegistry& registry, const void* tag) {
    for (const EnumNode* n = registry.enums; n != nullptr; n = n->next) {
        if (n->info->tag == tag) return n->info;
    }
    return nullptr;
}

const EnumInfo* find_enum_by_name(const DataRegistry& registry, const char* name) {
    for (const EnumNode* n = registry.enums; n != nullptr; n = n->next) {
        if (std::strcmp(n->info->name, name) == 0) return n->info;
    }
    return nullptr;
}

int count_types(const DataRegistry& registry) {
    int count = 0;
    for (const TypeNode* n = registry.types; n != nullptr; n = n->next) ++count;
    return count;
}

int count_enums(const DataRegistry& registry) {
    int count = 0;
    for (const EnumNode* n = registry.enums; n != nullptr; n = n->next) ++count;
    return count;
}

} // namespace nnh::detail
