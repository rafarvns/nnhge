// Registering a prefab (spec 021 FR-2, FR-3, FR-15): every check before anything is written — the name and its hash,
// the values, the flattened result (prefab_flatten.cpp), the capacities with each value's alignment — then the
// record, the entries, the values copy-constructed in call order, the hash in the table and the name
// (Debug/Development).

#include "prefabs/prefab_internal.hpp"

#include <new>

namespace nnh {
namespace detail {
namespace {

bool fail(Error& error, ErrorCode code, const char* what) {
    error = Error::make(code, "PrefabLibrary::add: %s", what);
    return false;
}

bool check_values(const ComponentValue* values, int count, Error& error) {
    if (count < 0 || (count > 0 && values == nullptr)) return fail(error, ErrorCode::InvalidArgument, "no values");
    if (count > kMaxPrefabEntries) return fail(error, ErrorCode::LimitReached, "more than 32 entries");
    for (int j = 0; j < count; ++j) {
        const ComponentValue& v = values[j];
        const bool power_of_two = v.align > 0 && (v.align & (v.align - 1)) == 0;
        if (v.id == nullptr || v.value == nullptr || v.copy == nullptr || v.size < 1 || !power_of_two || v.align > 16)
            return fail(error, ErrorCode::InvalidArgument, "a value not made by nnh::component_value()");
        for (int k = 0; k < j; ++k)
            if (values[k].id == v.id) return fail(error, ErrorCode::InvalidArgument, "a kind twice among the values");
    }
    return true;
}

bool check_capacities(const PrefabBlock& b, const ComponentValue* values, int count, PrefabPlan& plan, Error& error) {
    if (b.prefabs >= b.max_prefabs) return fail(error, ErrorCode::LimitReached, "the library's max_prefabs is full");
    if (b.entries_used + plan.entries > b.max_entries)
        return fail(error, ErrorCode::LimitReached, "the library's max_entries is full");
    std::uint32_t at = static_cast<std::uint32_t>(b.value_used);
    for (int j = 0; j < count; ++j) {
        const auto align = static_cast<std::uint32_t>(values[j].align);
        at = (at + align - 1) & ~(align - 1);
        plan.offsets[j] = at;
        at += static_cast<std::uint32_t>(values[j].size);
    }
    if (at > static_cast<std::uint32_t>(b.value_capacity))
        return fail(error, ErrorCode::LimitReached, "the library's value_bytes is full");
    plan.value_end = static_cast<int>(at);
    return true;
}

void commit(PrefabBlock& b, const char* name, const PrefabOptions& options, const ComponentValue* values, int count,
            const PrefabPlan& plan) {
    const int index = b.prefabs;
    PrefabRecord* r = ::new (static_cast<void*>(b.records + index)) PrefabRecord{};
    r->first = static_cast<std::uint16_t>(b.entries_used);
    r->entries = static_cast<std::uint8_t>(plan.entries);
    r->tag_count = static_cast<std::uint8_t>(plan.tag_count);
    for (int i = 0; i < plan.tag_count; ++i) r->tags[i] = plan.tags[i];
    r->place_entry = plan.place_entry;
    r->place_flags = plan.place_flags;
    r->place_offset = plan.place_offset;
    r->hook_base = plan.hook_base;
    r->hooks = plan.hooks;
    r->hook = options.on_spawn;
    const PrefabRecord* base = plan.base >= 0 ? b.records + plan.base : nullptr;
    for (int i = 0; i < plan.entries; ++i) {
        PrefabEntry& e = b.entries[b.entries_used + i];
        const int j = plan.own[i];
        if (j < 0) { // inherited: shares the base's value, which the base destroys
            e = b.entries[base->first + i];
            e.value &= kValueOffset;
        } else {
            e = {values[j].id, plan.offsets[j] | kOwnsValue, values[j].copy, values[j].destroy};
        }
    }
    for (int j = 0; j < count; ++j) values[j].copy(b.values + plan.offsets[j], values[j].value); // call order
    b.hashes[index] = plan.hash;
    b.table[table_probe(b, plan.hash)] = static_cast<std::uint16_t>(index + 1);
    if (b.names != nullptr) {
        char* out = b.names + index * kNameBytes;
        int n = 0;
        for (; n < kNameBytes - 1 && name[n] != '\0'; ++n) out[n] = name[n];
        out[n] = '\0';
    }
    b.prefabs = index + 1;
    b.entries_used += plan.entries;
    b.value_used = plan.value_end;
}

} // namespace
} // namespace detail

Result<Prefab> PrefabLibrary::add(const char* name, const PrefabOptions& options, const ComponentValue* values,
                                  int count) {
    detail::PrefabBlock* b = block_;
    if (b == nullptr) {
        detail::prefab_misuse("PrefabLibrary::add() on an empty or moved-from library");
        return Error::make(ErrorCode::InvalidArgument, "PrefabLibrary::add: empty library");
    }
    Error error{};
    if (name == nullptr || name[0] == '\0') return Error::make(ErrorCode::InvalidArgument, "PrefabLibrary::add: no name");
    detail::PrefabPlan plan;
    plan.hash = detail::hash_string(name);
    if (b->table[detail::table_probe(*b, plan.hash)] != 0)
        return Error::make(ErrorCode::InvalidArgument, "PrefabLibrary::add: \"%s\": the name's hash is registered", name);
    if (!detail::check_values(values, count, error) || !detail::flatten(*b, options, values, count, plan, error) ||
        !detail::check_capacities(*b, values, count, plan, error))
        return error;
    detail::commit(*b, name, options, values, count, plan);
    return detail::prefab_at(*b, b->prefabs - 1);
}

} // namespace nnh
