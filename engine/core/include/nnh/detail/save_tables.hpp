#pragma once
/// @file nnh/detail/save_tables.hpp
/// Builders of the save records behind the NNH_SAVE_* macros (spec 027, D45): one entry per macro argument,
/// then the fields sorted by id, the retired ids, the steps, the checks and the largest encoded sizes, all at
/// compile time. Included by <nnh/saves.hpp>; never include it on its own.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <new>
#include <type_traits>

namespace nnh::detail {

inline constexpr std::size_t kSaveHeaderBytes = 16; ///< the structured header before its key bytes

enum class SaveEntryKind : std::uint8_t { Field, Retired, Version, Step };

/// One argument of a save declaration (a field, retired ids, the version or a step).
struct SaveEntry {
    SaveEntryKind what = SaveEntryKind::Field;
    SaveFieldInfo field{};
    std::uint32_t number = 0; ///< Version: the version; Step: the version it converts from
    void (*step)(void*, const save::OldFields&) = nullptr;
    std::uint16_t retired[save::kMaxRetired] = {};
    int retired_count = 0;
};

/// A fixed array of at least one element (constexpr tables can't have zero-length arrays).
template <class T, int N> struct SaveArray {
    T v[N > 0 ? N : 1] = {};
};

/// What a declaration lists.
struct SaveCounts {
    int fields = 0;
    int retired = 0;
    int steps = 0;
    int versions = 0;
    std::uint32_t version = 1;
};

/// NNH_SAVE_FIELD: member type M at `offset` with field id `id`.
template <class M> constexpr SaveEntry save_field_entry(const char* name, std::size_t offset, long long id) {
    static_assert(!std::is_reference_v<M>, "saved fields can't be pointers or references");
    using Shape = SaveShape<std::remove_cv_t<M>>;
    using E = typename Shape::Element;
    if constexpr (std::is_const_v<M> || std::is_volatile_v<M>) {
        static_assert(save_always_false<M>,
                      "saved fields can't be const or volatile: the loader writes them");
    }
    save_check_element<E>();
    if (id < 1 || id > save::kMaxId) save_error_field_id_must_be_1_to_65535();
    SaveEntry e;
    SaveFieldInfo& f = e.field;
    f.name = name;
    f.offset = static_cast<std::uint32_t>(offset);
    f.stride = static_cast<std::uint32_t>(sizeof(E));
    f.items_offset = static_cast<std::uint32_t>(Shape::items_offset);
    f.count = static_cast<std::uint16_t>(Shape::count);
    f.id = static_cast<std::uint16_t>(id);
    f.capacity = static_cast<std::uint8_t>(DataStringCapacity<E>::value);
    f.kind = save_kind_of<E>();
    f.shape = Shape::shape;
    f.width = std::is_arithmetic_v<E> || std::is_enum_v<E> ? static_cast<std::uint8_t>(sizeof(E)) : 0;
    f.nested = save_nested_of<E>();
    f.enum_names = save_enum_lookup_of<E>();
    return e;
}

/// NNH_SAVE_RETIRED: ids of fields removed in earlier versions (never to be reused).
constexpr SaveEntry save_retired_entry(std::initializer_list<long long> ids) {
    SaveEntry e;
    e.what = SaveEntryKind::Retired;
    if (ids.size() > static_cast<std::size_t>(save::kMaxRetired)) save_error_more_than_64_retired_ids();
    for (long long id : ids) {
        if (id < 1 || id > save::kMaxId) save_error_retired_id_must_be_1_to_65535();
        e.retired[e.retired_count++] = static_cast<std::uint16_t>(id);
    }
    return e;
}

/// NNH_SAVE_VERSION: the type's current save version (1 when not given).
constexpr SaveEntry save_version_entry(long long version) {
    if (version < 1 || version > 0xFFFFFFFFll) save_error_version_must_be_at_least_1();
    SaveEntry e;
    e.what = SaveEntryKind::Version;
    e.number = static_cast<std::uint32_t>(version);
    return e;
}

template <class T, auto F> void save_step_thunk(void* value, const save::OldFields& old) {
    F(*static_cast<T*>(value), old);
}

/// NNH_SAVE_STEP: F converts a value of version `from` into version from + 1.
template <class T, auto F> constexpr SaveEntry save_step_entry(long long from) {
    static_assert(std::is_invocable_v<decltype(F), T&, const save::OldFields&>,
                  "a migration step is a function `void step(T& value, const nnh::save::OldFields& old)`");
    if (from < 1 || from > 0xFFFFFFFFll) save_error_step_from_a_version_not_below_the_current_one();
    SaveEntry e;
    e.what = SaveEntryKind::Step;
    e.number = static_cast<std::uint32_t>(from);
    e.step = &save_step_thunk<T, F>;
    return e;
}

template <std::size_t M> constexpr SaveCounts save_count(const SaveEntry (&entries)[M]) {
    SaveCounts c;
    for (const SaveEntry& e : entries) {
        if (e.what == SaveEntryKind::Field) ++c.fields;
        if (e.what == SaveEntryKind::Retired) c.retired += e.retired_count;
        if (e.what == SaveEntryKind::Step) ++c.steps;
        if (e.what == SaveEntryKind::Version) {
            ++c.versions;
            c.version = e.number;
        }
    }
    return c;
}

template <int N, std::size_t M>
constexpr SaveArray<SaveFieldInfo, N> save_sorted_fields(const SaveEntry (&entries)[M]) {
    SaveArray<SaveFieldInfo, N> out;
    int n = 0;
    for (const SaveEntry& e : entries) {
        if (e.what != SaveEntryKind::Field) continue;
        int i = n++;
        for (; i > 0 && out.v[i - 1].id > e.field.id; --i)
            out.v[i] = out.v[i - 1];
        out.v[i] = e.field;
    }
    return out;
}

template <int N, std::size_t M>
constexpr SaveArray<std::uint16_t, N> save_sorted_retired(const SaveEntry (&entries)[M]) {
    SaveArray<std::uint16_t, N> out;
    int n = 0;
    for (const SaveEntry& e : entries) {
        if (e.what != SaveEntryKind::Retired) continue;
        for (int k = 0; k < e.retired_count && n < N; ++k) {
            int i = n++;
            for (; i > 0 && out.v[i - 1] > e.retired[k]; --i)
                out.v[i] = out.v[i - 1];
            out.v[i] = e.retired[k];
        }
    }
    return out;
}

template <int N, std::size_t M>
constexpr SaveArray<SaveStep, N> save_sorted_steps(const SaveEntry (&entries)[M]) {
    SaveArray<SaveStep, N> out;
    int n = 0;
    for (const SaveEntry& e : entries) {
        if (e.what != SaveEntryKind::Step) continue;
        int i = n++;
        for (; i > 0 && out.v[i - 1].from > e.number; --i)
            out.v[i] = out.v[i - 1];
        out.v[i] = SaveStep{e.number, e.step};
    }
    return out;
}

constexpr std::uint64_t save_varint_size(std::uint64_t v) {
    return v < 0x80u ? 1 : v < 0x4000u ? 2 : 3;
}

constexpr int save_fixed_width(save::WireKind kind) {
    switch (kind) {
    case save::WireKind::Bool:
    case save::WireKind::U8:
    case save::WireKind::I8:
        return 1;
    case save::WireKind::U16:
    case save::WireKind::I16:
        return 2;
    case save::WireKind::U64:
    case save::WireKind::I64:
    case save::WireKind::F64:
    case save::WireKind::Vec2:
        return 8;
    case save::WireKind::String:
    case save::WireKind::Struct:
    case save::WireKind::None:
        return 0;
    default:
        return 4;
    }
}

/// The largest encoding of one field's value (no tag): every string and list full.
constexpr std::uint64_t save_max_value(const SaveFieldInfo& f) {
    const std::uint64_t width = static_cast<std::uint64_t>(save_fixed_width(f.kind));
    const std::uint64_t string = 1u + f.capacity;
    if (f.shape == save::WireShape::One) {
        if (f.kind == save::WireKind::String) return string;
        if (f.kind == save::WireKind::Struct)
            return save_varint_size(f.nested->max_fields) + f.nested->max_fields;
        return width;
    }
    const std::uint64_t n = f.count;
    std::uint64_t size = save_varint_size(n);
    if (f.kind == save::WireKind::String) return size + save_varint_size(n * string) + n * string;
    if (f.kind == save::WireKind::Struct) {
        const std::uint64_t rows = n * f.nested->max_row;
        return size + f.nested->template_size + save_varint_size(rows) + rows;
    }
    return size + n * width;
}

constexpr bool save_key_ok(const char* key) {
    if (!(key[0] >= 'a' && key[0] <= 'z')) return false;
    int n = 0;
    for (; key[n] != '\0'; ++n) {
        const char c = key[n];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') || n >= save::kMaxKeyLength)
            return false;
    }
    return true;
}

constexpr std::uint32_t save_key_hash(const char* key) {
    std::uint32_t hash = 0x811C9DC5u; // FNV-1a 32, as detail::fnv1a32
    for (; *key != '\0'; ++key) {
        hash ^= static_cast<std::uint8_t>(*key);
        hash *= 0x01000193u;
    }
    return hash;
}

constexpr std::size_t save_key_length(const char* key) {
    std::size_t n = 0;
    while (key[n] != '\0')
        ++n;
    return n;
}

constexpr void save_check_steps(const SaveCounts& c, const SaveStep* steps, SaveRole role) {
    if (role != SaveRole::Type) {
        if (c.versions > 0 || c.steps > 0)
            save_error_NNH_SAVE_VERSION_or_NNH_SAVE_STEP_in_a_struct_or_summary();
        return;
    }
    if (c.versions > 1) save_error_version_given_twice();
    for (int i = 0; i < c.steps; ++i) {
        if (steps[i].from >= c.version) save_error_step_from_a_version_not_below_the_current_one();
        if (i > 0 && steps[i].from == steps[i - 1].from) save_error_two_steps_for_one_version();
    }
    for (int i = 0; i < c.steps; ++i) {
        if (steps[i].from != static_cast<std::uint32_t>(i) + 1) {
            save_error_missing_migration_step_every_version_below_the_current_one_needs_one();
        }
    }
    if (static_cast<std::uint32_t>(c.steps) + 1 != c.version) {
        save_error_missing_migration_step_every_version_below_the_current_one_needs_one();
    }
}

constexpr void save_check_ids(const SaveCounts& c, const SaveFieldInfo* fields,
                              const std::uint16_t* retired) {
    if (c.fields == 0) save_error_a_save_struct_needs_a_field();
    if (c.fields > save::kMaxFields) save_error_more_than_128_fields();
    if (c.retired > save::kMaxRetired) save_error_more_than_64_retired_ids();
    for (int i = 1; i < c.fields; ++i) {
        if (fields[i].id == fields[i - 1].id) save_error_field_id_used_twice();
    }
    for (int i = 1; i < c.retired; ++i) {
        if (retired[i] == retired[i - 1]) save_error_retired_id_listed_twice();
    }
    for (int i = 0, k = 0; i < c.fields && k < c.retired;) { // both sorted: a merge
        if (fields[i].id == retired[k]) save_error_retired_id_used_by_a_field();
        if (fields[i].id < retired[k])
            ++i;
        else
            ++k;
    }
}

/// The record of save type, struct or summary T. See NNH_SAVE_TYPE.
template <class T, int F, int R, int S>
constexpr SaveTypeInfo
make_save_type(const char* name, const char* key, SaveRole role, long long summary_version,
               const SaveCounts& c, const SaveArray<SaveFieldInfo, F>& fields,
               const SaveArray<std::uint16_t, R>& retired, const SaveArray<SaveStep, S>& steps) {
    static_assert(std::is_class_v<T> && std::is_aggregate_v<T>,
                  "a save type must be a plain struct (an aggregate: no constructors of its own, no virtual "
                  "functions)");
    static_assert(std::is_standard_layout_v<T>,
                  "a save type must be standard-layout (all members public, no virtual)");
    static_assert(std::is_trivially_copyable_v<T>, "a save type must be trivially copyable");
    static_assert(sizeof(T) <= save::kMaxRootBytes,
                  "a save type can be at most 64 KiB (save::kMaxRootBytes)");
    save_check_ids(c, fields.v, retired.v);
    save_check_steps(c, steps.v, role);
    SaveTypeInfo t;
    t.name = name;
    t.key = key;
    t.role = role;
    if (role != SaveRole::Struct && !save_key_ok(key)) {
        save_error_bad_key_use_1_to_16_of_a_to_z_0_to_9_underscore_starting_with_a_letter();
    }
    t.version = role == SaveRole::Type      ? c.version
                : role == SaveRole::Summary ? static_cast<std::uint32_t>(summary_version)
                                            : 0;
    if (role == SaveRole::Summary && summary_version < 1) save_error_version_must_be_at_least_1();
    t.key_hash = role == SaveRole::Struct ? 0 : save_key_hash(key);
    t.fields = fields.v;
    t.retired = retired.v;
    t.steps = steps.v;
    t.field_count = static_cast<std::uint16_t>(c.fields);
    t.retired_count = static_cast<std::uint16_t>(c.retired);
    t.step_count = static_cast<std::uint16_t>(c.steps);
    t.size = static_cast<std::uint32_t>(sizeof(T));
    t.reset = [](void* p) { ::new (p) T{}; };
    std::uint64_t all = 0, row = 0, tmpl = save_varint_size(static_cast<std::uint64_t>(c.fields));
    int depth = 1;
    for (int i = 0; i < c.fields; ++i) {
        const SaveFieldInfo& f = fields.v[i];
        const std::uint64_t value = save_max_value(f);
        all += 1 + save_varint_size(f.id) + value;
        row += value;
        tmpl += 1 + save_varint_size(f.id);
        if (f.nested != nullptr && f.nested->depth + 1 > depth) depth = f.nested->depth + 1;
    }
    if (depth > save::kMaxDepth) save_error_nesting_deeper_than_8_levels();
    const std::uint64_t root = role == SaveRole::Struct ? 0 : kSaveHeaderBytes + save_key_length(key);
    if (root + all > save::kMaxEncodedSize || row > save::kMaxEncodedSize)
        save_error_max_encoded_size_over_256_KiB();
    t.max_fields = static_cast<std::uint32_t>(all);
    t.max_row = static_cast<std::uint32_t>(row);
    t.template_size = static_cast<std::uint32_t>(tmpl);
    t.depth = static_cast<std::uint8_t>(depth);
    return t;
}

} // namespace nnh::detail
