// Creating and destroying prefab libraries (spec 021 FR-1, FR-15): validation (Result errors, never asserts), one block
// of tagged memory laid out by library_layout(), a stamp per library, moves, and a library destroyed inside one of its
// own spawn hooks (misuse: freed when the outermost spawn returns).

#include "nnh/assert.hpp"
#include "nnh/build.hpp"
#include "prefabs/library_layout.hpp"
#include "prefabs/prefab_internal.hpp"

#include <new>

NNH_MEM_TAG(prefabs);
NNH_LOG_CHANNEL(prefabs);

namespace nnh {
namespace detail {
namespace {

std::uint16_t g_last_stamp = 0; // stamps go 1, 2, ..., 65535, 1, ...: a handle is never null

std::uint16_t next_stamp() {
    g_last_stamp = static_cast<std::uint16_t>(g_last_stamp == 0xFFFFu ? 1u : g_last_stamp + 1u);
    return g_last_stamp;
}

bool invalid(Error& error, const char* what, int value) {
    error = Error::make(ErrorCode::InvalidArgument, "PrefabLibrary::create: %s (%d)", what, value);
    return false;
}

bool validate(const PrefabLibraryDesc& desc, Error& error) {
    if (desc.max_prefabs < 1 || desc.max_prefabs > kMaxPrefabs)
        return invalid(error, "max_prefabs outside 1..1024", desc.max_prefabs);
    if (desc.max_entries < 0 || desc.max_entries > 65535)
        return invalid(error, "max_entries outside 0..65535", desc.max_entries);
    if (desc.value_bytes < 0 || desc.value_bytes > (1 << 20))
        return invalid(error, "value_bytes outside 0..1 MiB", desc.value_bytes);
    if (desc.max_children < 0 || desc.max_children > 65535)
        return invalid(error, "max_children outside 0..65535", desc.max_children);
    return true;
}

PrefabBlock* build(const PrefabLibraryDesc& desc, const LibraryLayout& l, unsigned char* bytes) {
    auto* b = ::new (bytes) PrefabBlock{};
    b->hashes = reinterpret_cast<std::uint32_t*>(bytes + l.hashes);
    b->table = reinterpret_cast<std::uint16_t*>(bytes + l.table);
    b->records = reinterpret_cast<PrefabRecord*>(bytes + l.records);
    b->names = build::kDevToolsEnabled ? reinterpret_cast<char*>(bytes + l.names) : nullptr;
    b->entries = reinterpret_cast<PrefabEntry*>(bytes + l.entries);
    b->values = bytes + l.values;
    for (int i = 0; i < desc.max_prefabs; ++i) b->hashes[i] = 0;
    for (int i = 0; i < l.table_size; ++i) b->table[i] = 0;
    b->table_mask = static_cast<std::uint32_t>(l.table_size - 1);
    b->max_prefabs = desc.max_prefabs;
    b->max_entries = desc.max_entries;
    b->value_capacity = desc.value_bytes;
    b->stamp = next_stamp();
    b->when_full = desc.when_full;
    if (desc.max_children > 0) {
        b->children = reinterpret_cast<PrefabChildEntry*>(bytes + l.children);
        b->facts = reinterpret_cast<PrefabTreeFacts*>(bytes + l.facts);
        for (int i = 0; i < desc.max_children; ++i) ::new (static_cast<void*>(b->children + i)) PrefabChildEntry{};
        for (int i = 0; i < desc.max_prefabs; ++i) ::new (static_cast<void*>(b->facts + i)) PrefabTreeFacts{};
        b->max_children = desc.max_children;
    }
    return b;
}

} // namespace

MemTag& prefabs_memory_tag() { return NNH_MEM_TAG_REF(prefabs); }

void prefab_misuse(const char* what) {
    NNH_LOG_ERROR(prefabs, "%s", what);
    NNH_ASSERT(false, "%s", what);
}

void library_teardown(PrefabBlock* b) {
    for (int i = 0; i < b->entries_used; ++i) {
        const PrefabEntry& e = b->entries[i];
        if ((e.value & kOwnsValue) != 0 && e.destroy != nullptr) e.destroy(b->values + (e.value & kValueOffset));
    }
    mem::free(b);
}

} // namespace detail

Result<PrefabLibrary> PrefabLibrary::create(const PrefabLibraryDesc& desc) {
    Error error{};
    if (!detail::validate(desc, error)) return error;
    const detail::LibraryLayout l = detail::library_layout(desc, build::kDevToolsEnabled);
    MemTag& tag = desc.memory_tag != nullptr ? *desc.memory_tag : detail::prefabs_memory_tag();
    auto* bytes = static_cast<unsigned char*>(mem::alloc(MemRegion::Heap, tag, l.total, 16));
    if (bytes == nullptr) return Error::make(ErrorCode::OutOfMemory, "PrefabLibrary::create: no memory for %zu bytes", l.total);
    PrefabLibrary library;
    library.block_ = detail::build(desc, l, bytes);
    return Result<PrefabLibrary>(static_cast<PrefabLibrary&&>(library));
}

PrefabLibrary::~PrefabLibrary() {
    detail::PrefabBlock* b = block_;
    if (b == nullptr) return;
    if (b->spawn_depth > 0) { // Debug stops here, the library untouched; Release frees it after the outermost spawn
        detail::prefab_misuse("nnh::PrefabLibrary destroyed inside one of its own spawn hooks");
        block_ = nullptr;
        b->doomed = true;
        return;
    }
    block_ = nullptr;
    detail::library_teardown(b);
}

PrefabLibrary::PrefabLibrary(PrefabLibrary&& other) noexcept : block_(other.block_) { other.block_ = nullptr; }

PrefabLibrary& PrefabLibrary::operator=(PrefabLibrary&& other) noexcept {
    if (this == &other) return *this;
    if (block_ != nullptr && block_->spawn_depth > 0) // Debug stops here, the library untouched
        detail::prefab_misuse("nnh::PrefabLibrary replaced inside one of its own spawn hooks");
    PrefabLibrary old; // its destructor frees the replaced block
    old.block_ = block_;
    block_ = other.block_;
    other.block_ = nullptr;
    return *this;
}

} // namespace nnh
