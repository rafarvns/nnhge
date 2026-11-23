// A prefab library's block layout (spec 021 FR-1): header, hashes, the name table, records, names (Debug/Development),
// entries, values. Every part aligned to its type, the values to 16 (the most a prefab value may need).

#include "prefabs/library_layout.hpp"

#include "prefabs/prefab_internal.hpp"

namespace nnh::detail {
namespace {

std::size_t align_up(std::size_t value, std::size_t align) { return (value + align - 1) & ~(align - 1); }

} // namespace

LibraryLayout library_layout(const PrefabLibraryDesc& desc, bool names) {
    LibraryLayout l{};
    const std::size_t prefabs = static_cast<std::size_t>(desc.max_prefabs);
    l.table_size = 1;
    while (l.table_size < 2 * desc.max_prefabs) l.table_size *= 2;
    std::size_t at = sizeof(PrefabBlock);
    at = l.hashes = align_up(at, alignof(std::uint32_t));
    at = l.table = at + 4 * prefabs;
    at = l.records = align_up(at + 2 * static_cast<std::size_t>(l.table_size), alignof(PrefabRecord));
    at = l.names = at + sizeof(PrefabRecord) * prefabs;
    if (names) at += static_cast<std::size_t>(kNameBytes) * prefabs;
    at = l.entries = align_up(at, alignof(PrefabEntry));
    at = l.values = align_up(at + sizeof(PrefabEntry) * static_cast<std::size_t>(desc.max_entries), 16);
    l.total = at + static_cast<std::size_t>(desc.value_bytes);
    return l;
}

} // namespace nnh::detail
