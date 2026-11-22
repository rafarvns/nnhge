// The schema description writer (schema_write.hpp). Pure.

#include "data/schema_write.hpp"

#include <cfloat>
#include <cstdio>
#include <cstring>

#include "data/enum_names.hpp"
#include "data/schema_fingerprint.hpp"

namespace nnh::detail {

namespace {

struct Out {
    TextSink sink;
    void* context;

    void text(const char* s, std::size_t n) const { sink(context, s, n); }
    void text(const char* s) const { text(s, std::strlen(s)); }
    void number(long long v) const {
        char buffer[24];
        text(buffer, static_cast<std::size_t>(std::snprintf(buffer, sizeof buffer, "%lld", v)));
    }
};

/// The node after `previous` in name order (the first one when `previous` is null): sorting without memory.
template <class Node> const Node* next_by_name(const Node* list, const char* previous) {
    const Node* best = nullptr;
    for (const Node* n = list; n != nullptr; n = n->next) {
        const char* name = n->info->name;
        if (previous != nullptr && std::strcmp(name, previous) <= 0) continue;
        if (best == nullptr || std::strcmp(name, best->info->name) < 0) best = n;
    }
    return best;
}

void write_enum(const Out& out, const EnumInfo& e) {
    out.text("enum ");
    out.text(e.name);
    out.text(" ");
    out.number(e.count);
    for (int i = 0; i < e.count; ++i) {
        const NameView v = enum_name(e.names, i);
        out.text(" ");
        out.text(v.text, static_cast<std::size_t>(v.length));
    }
    out.text("\n");
}

const char* int_name(const FieldInfo& f) {
    switch (f.width) {
    case 1:
        return f.is_signed ? "i8" : "u8";
    case 2:
        return f.is_signed ? "i16" : "u16";
    default:
        return f.is_signed ? "i32" : "u32";
    }
}

/// The values the member's type holds, for deciding what min/max to write.
void type_range(const FieldInfo& f, double& lowest, double& highest) {
    if (f.kind == FieldKind::Float) {
        lowest = -static_cast<double>(FLT_MAX);
        highest = static_cast<double>(FLT_MAX);
        return;
    }
    const int bits = 8 * f.width;
    const double span = static_cast<double>(1ull << bits);
    lowest = f.is_signed ? -span / 2 : 0;
    highest = f.is_signed ? span / 2 - 1 : span - 1;
}

void write_bound(const Out& out, const char* label, const FieldInfo& f, double v) {
    out.text(label);
    if (f.kind == FieldKind::Int) {
        out.number(static_cast<long long>(v));
        return;
    }
    char buffer[40];
    out.text(buffer, static_cast<std::size_t>(std::snprintf(buffer, sizeof buffer, "%.17g", v)));
}

void write_kind(const Out& out, const DataRegistry& r, const FieldInfo& f) {
    static constexpr const char* kPlain[] = {"bool",  "", "float", "",        "",      "vec2",
                                             "color", "", "",      "texture", "sound", "music"};
    const auto k = static_cast<int>(f.kind);
    if (f.kind == FieldKind::Int) {
        out.text(int_name(f));
    } else if (f.kind == FieldKind::String) {
        out.text("string ");
        out.number(f.capacity);
    } else if (f.kind == FieldKind::Enum) {
        const EnumInfo* e = find_enum(r, f.target);
        out.text("enum ");
        out.text(e != nullptr ? e->name : "?");
    } else if (f.kind == FieldKind::Struct || f.kind == FieldKind::DataRef) {
        const TypeInfo* t = find_type(r, f.target);
        out.text(f.kind == FieldKind::Struct ? "struct " : "ref ");
        out.text(t != nullptr ? t->name : "?");
    } else if (k >= 0 && k < static_cast<int>(sizeof kPlain / sizeof kPlain[0])) {
        out.text(kPlain[k]);
    }
}

void write_field(const Out& out, const DataRegistry& r, const FieldInfo& f) {
    out.text("field ");
    out.text(f.name);
    if (f.shape == FieldShape::One) {
        out.text(" one ");
    } else {
        out.text(f.shape == FieldShape::Array ? " array " : " list ");
        out.number(f.count);
        out.text(" ");
    }
    write_kind(out, r, f);
    if (f.kind == FieldKind::Int || f.kind == FieldKind::Float) {
        double lowest = 0;
        double highest = 0;
        type_range(f, lowest, highest);
        if (f.min > lowest) write_bound(out, " min ", f, f.min);
        if (f.max < highest) write_bound(out, " max ", f, f.max);
    }
    if (f.required) out.text(" required");
    out.text("\n");
}

void write_type(const Out& out, const DataRegistry& r, const TypeInfo& t) {
    char head[48];
    out.text("type ");
    out.text(t.name);
    out.text(" ");
    out.text(t.key[0] != '\0' ? t.key : "-");
    const int n = std::snprintf(head, sizeof head, " %d %08X ", t.field_count,
                                static_cast<unsigned>(schema_fingerprint(r, t)));
    out.text(head, static_cast<std::size_t>(n));
    out.text(t.source);
    out.text("\n");
    for (int i = 0; i < t.field_count; ++i) write_field(out, r, t.fields[i]);
}

} // namespace

void write_schema(const DataRegistry& registry, TextSink sink, void* context) {
    const Out out{sink, context};
    out.text("nnh-data-schema ");
    out.number(kSchemaDescriptionVersion);
    out.text("\n");
    for (const EnumNode* n = next_by_name<EnumNode>(registry.enums, nullptr); n != nullptr;
         n = next_by_name(registry.enums, n->info->name)) {
        write_enum(out, *n->info);
    }
    for (const TypeNode* n = next_by_name<TypeNode>(registry.types, nullptr); n != nullptr;
         n = next_by_name(registry.types, n->info->name)) {
        write_type(out, registry, *n->info);
    }
}

} // namespace nnh::detail
