// Checking a whole data schema (schema_check.hpp). Pure.

#include "data/schema_check.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "data/data_format.hpp"
#include "data/enum_names.hpp"
#include "nnh/detail/printf_format.hpp"

namespace nnh::detail {

namespace {

using datafmt::kMaxNameLength;

struct Reporter {
    SchemaIssueSink sink;
    void* context;
    int count = 0;

    void operator()(const char* format, ...) NNH_PRINTF_FORMAT(2, 3) {
        char message[512];
        va_list args;
        va_start(args, format);
        std::vsnprintf(message, sizeof message, format, args);
        va_end(args);
        if (sink != nullptr) sink(context, message);
        ++count;
    }
};

// Extensions other asset steps own. D31 (spec 016): Aseprite files and converted sprite libraries too.
constexpr const char* kReservedKeys[] = {"png",      "wav", "ogg",       "tmj", "tsj",     "nnhmap",
                                         "nnh_data", "t3x", "nnhsprite", "ase", "aseprite"};

bool key_syntax_ok(const char* key) {
    const std::size_t n = std::strlen(key);
    if (n < 1 || n > static_cast<std::size_t>(datafmt::kMaxKeyLength) || key[0] < 'a' || key[0] > 'z')
        return false;
    for (std::size_t i = 0; i < n; ++i) {
        const char c = key[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

bool reserved_key(const char* key) {
    for (const char* r : kReservedKeys) {
        if (std::strcmp(r, key) == 0) return true;
    }
    return false;
}

bool too_long(const char* name) { return std::strlen(name) > static_cast<std::size_t>(kMaxNameLength); }

void check_type_identity(const DataRegistry& r, Reporter& report) {
    for (const TypeNode* a = r.types; a != nullptr; a = a->next) {
        const TypeInfo& t = *a->info;
        if (too_long(t.name))
            report("%s: type %.63s...: the name is longer than 63 characters", t.source, t.name);
        const bool has_key = t.key[0] != '\0';
        if (has_key && !key_syntax_ok(t.key)) {
            report("%s: type %s: key '%s' must be 1 to 16 characters among a-z, 0-9 and _, starting with a "
                   "letter",
                   t.source, t.name, t.key);
        } else if (has_key && reserved_key(t.key)) {
            report("%s: type %s: key '%s' is reserved (an extension the asset pipeline uses)", t.source,
                   t.name, t.key);
        }
        for (const TypeNode* b = a->next; b != nullptr; b = b->next) {
            const TypeInfo& u = *b->info;
            if (t.tag == u.tag) {
                report("%s: type %s is declared twice (also in %s)", t.source, t.name, u.source);
            } else if (std::strcmp(t.name, u.name) == 0) {
                report("%s: two types are named '%s' (also in %s): rename one", t.source, t.name, u.source);
            }
            if (t.tag != u.tag && has_key && std::strcmp(t.key, u.key) == 0) {
                report("%s: types %s and %s both use the key '%s' (also in %s)", t.source, t.name, u.name,
                       t.key, u.source);
            }
        }
    }
}

void check_field(const DataRegistry& r, const TypeInfo& t, const FieldInfo& f, Reporter& report) {
    if (too_long(f.name))
        report("%s: %s.%.63s...: the name is longer than 63 characters", t.source, t.name, f.name);
    if (f.kind == FieldKind::Struct && find_type(r, f.target) == nullptr) {
        report("%s: %s.%s: its type has no NNH_DATA_STRUCT or NNH_DATA_TYPE", t.source, t.name, f.name);
    } else if (f.kind == FieldKind::Enum && find_enum(r, f.target) == nullptr) {
        report("%s: %s.%s: its enum has no NNH_DATA_ENUM", t.source, t.name, f.name);
    } else if (f.kind == FieldKind::DataRef) {
        const TypeInfo* target = find_type(r, f.target);
        if (target == nullptr) {
            report("%s: %s.%s: Ref<T> needs a T declared with NNH_DATA_TYPE", t.source, t.name, f.name);
        } else if (target->key[0] == '\0') {
            report("%s: %s.%s: Ref<%s> needs a type with a key: %s is an NNH_DATA_STRUCT", t.source, t.name,
                   f.name, target->name, target->name);
        }
    }
}

int index_of(const DataRegistry& r, const void* tag) {
    int i = 0;
    for (const TypeNode* n = r.types; n != nullptr; n = n->next, ++i) {
        if (n->info->tag == tag) return i;
    }
    return -1;
}

/// Nested struct levels below each type (list order), capped at kMaxDepth + 1: one round per level, so cycles
/// stop.
void nesting_heights(const DataRegistry& r, int* height, int count) {
    for (int i = 0; i < count; ++i) height[i] = 0;
    for (int round = 0; round <= data::kMaxDepth; ++round) {
        bool changed = false;
        int i = 0;
        for (const TypeNode* n = r.types; n != nullptr; n = n->next, ++i) {
            for (int k = 0; k < n->info->field_count; ++k) {
                const FieldInfo& f = n->info->fields[k];
                const int j = f.kind == FieldKind::Struct ? index_of(r, f.target) : -1;
                if (j < 0 || height[j] + 1 <= height[i]) continue;
                height[i] = height[j] + 1;
                changed = true;
            }
        }
        if (!changed) break;
    }
}

void check_depth(const DataRegistry& r, Reporter& report) {
    const int count = count_types(r);
    if (count > datafmt::kMaxTypes) return; // reported by the counts
    int height[datafmt::kMaxTypes];
    nesting_heights(r, height, count);
    int i = 0;
    for (const TypeNode* n = r.types; n != nullptr; n = n->next, ++i) {
        if (n->info->key[0] == '\0' || height[i] <= data::kMaxDepth) continue;
        report("%s: type %s: structs nested %d deep, at most %d", n->info->source, n->info->name, height[i],
               data::kMaxDepth);
    }
}

void check_enums(const DataRegistry& r, Reporter& report) {
    for (const EnumNode* a = r.enums; a != nullptr; a = a->next) {
        const EnumInfo& e = *a->info;
        if (too_long(e.name))
            report("%s: enum %.63s...: the name is longer than 63 characters", e.source, e.name);
        for (const EnumNode* b = a->next; b != nullptr; b = b->next) {
            if (b->info->tag == e.tag) {
                report("%s: enum %s is declared twice (also in %s)", e.source, e.name, b->info->source);
            } else if (std::strcmp(b->info->name, e.name) == 0) {
                report("%s: two enums are named '%s' (also in %s): rename one", e.source, e.name,
                       b->info->source);
            }
        }
        for (int i = 0; i < e.count; ++i) {
            const NameView v = enum_name(e.names, i);
            if (v.length > kMaxNameLength) {
                report("%s: enum %s: name %d is longer than 63 characters", e.source, e.name, i);
            } else if (enum_index(e.names, v.text, v.length) != i) {
                report("%s: enum %s: the name '%.*s' is listed twice", e.source, e.name, v.length, v.text);
            }
        }
    }
}

void check_counts(const DataRegistry& r, Reporter& report) {
    int keyed = 0;
    for (const TypeNode* n = r.types; n != nullptr; n = n->next) keyed += n->info->key[0] != '\0' ? 1 : 0;
    if (keyed > datafmt::kMaxDataTypes) {
        report("schema: the schema declares %d data types: at most %d", keyed, datafmt::kMaxDataTypes);
    }
    const int types = count_types(r);
    if (types > datafmt::kMaxTypes) {
        report("schema: the schema declares %d types and structs: at most %d", types, datafmt::kMaxTypes);
    }
    const int enums = count_enums(r);
    if (enums > datafmt::kMaxEnums)
        report("schema: the schema declares %d enums: at most %d", enums, datafmt::kMaxEnums);
}

} // namespace

int check_schema(const DataRegistry& registry, SchemaIssueSink sink, void* context) {
    Reporter report{sink, context};
    check_type_identity(registry, report);
    for (const TypeNode* n = registry.types; n != nullptr; n = n->next) {
        for (int i = 0; i < n->info->field_count; ++i)
            check_field(registry, *n->info, n->info->fields[i], report);
    }
    check_depth(registry, report);
    check_enums(registry, report);
    check_counts(registry, report);
    return report.count;
}

} // namespace nnh::detail
