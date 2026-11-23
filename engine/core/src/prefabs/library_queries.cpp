// A library's queries and stats (spec 021 FR-11): an empty library or an invalid prefab answers nothing, without
// asserting (as spec 020's empty worlds).

#include "prefabs/prefab_internal.hpp"

namespace nnh {

int PrefabLibrary::count() const { return block_ != nullptr ? block_->prefabs : 0; }

int PrefabLibrary::capacity() const { return block_ != nullptr ? block_->max_prefabs : 0; }

bool PrefabLibrary::valid(Prefab prefab) const { return detail::record_of(block_, prefab) != nullptr; }

const char* PrefabLibrary::name(Prefab prefab) const {
    if (detail::record_of(block_, prefab) == nullptr || block_->names == nullptr) return "";
    return block_->names + prefab.slot() * detail::kNameBytes;
}

int PrefabLibrary::kinds(Prefab prefab, KindId* out, int capacity) const {
    const detail::PrefabRecord* r = detail::record_of(block_, prefab);
    if (r == nullptr) return 0;
    for (int i = 0; i < r->entries && i < capacity; ++i) out[i] = block_->entries[r->first + i].id;
    return r->entries;
}

int PrefabLibrary::tags(Prefab prefab, Tag* out, int capacity) const {
    const detail::PrefabRecord* r = detail::record_of(block_, prefab);
    if (r == nullptr) return 0;
    for (int i = 0; i < r->tag_count && i < capacity; ++i) out[i] = r->tags[i];
    return r->tag_count;
}

const void* PrefabLibrary::stored_value(Prefab prefab, KindId kind) const {
    const detail::PrefabRecord* r = detail::record_of(block_, prefab);
    if (r == nullptr) return nullptr;
    for (int i = 0; i < r->entries; ++i) {
        const detail::PrefabEntry& e = block_->entries[r->first + i];
        if (e.id == kind) return block_->values + (e.value & detail::kValueOffset);
    }
    return nullptr;
}

PrefabStats PrefabLibrary::stats() const {
    if (block_ == nullptr) return {};
    PrefabStats s = block_->counts;
    s.prefabs = block_->prefabs;
    s.capacity = block_->max_prefabs;
    s.entries = block_->entries_used;
    s.entry_capacity = block_->max_entries;
    s.value_bytes = block_->value_used;
    s.value_capacity = block_->value_capacity;
    return s;
}

} // namespace nnh
