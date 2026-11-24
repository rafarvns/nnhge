// Prefab names (spec 021 FR-3, FR-14): the FNV-1a 32 hash in one pass, and an open-addressing table of u16 indices
// (a power of two ≥ 2 × max_prefabs slots, linear probing, never more than half full) over a separate hash array, so a
// find touches 4 bytes per probe and never a record.

#include "prefabs/prefab_internal.hpp"

namespace nnh {
namespace detail {

namespace {

/// FNV-1a's prime in a register the compiler can't see into (plan 021, optimisation round). As a constant, GCC turns
/// `× 0x01000193` into five dependent shift-adds, and on the ARM11 each waits a cycle for its shifted operand: ≈ 11
/// cycles per byte on the hash's dependency chain. One `mul` is ≈ 4, and the next byte's load and test fit under it.
inline std::uint32_t fnv_prime() {
    std::uint32_t prime = 0x01000193u;
#if defined(__GNUC__) || defined(__clang__)
    asm("" : "+r"(prime));
#endif
    return prime;
}

} // namespace

std::uint32_t hash_string(const char* text) {
    const std::uint32_t prime = fnv_prime();
    std::uint32_t hash = 0x811C9DC5u;
    for (const auto* p = reinterpret_cast<const unsigned char*>(text); *p != 0; ++p) hash = (hash ^ *p) * prime;
    return hash;
}

std::uint32_t hash_bytes(const char* text, std::size_t size) {
    const std::uint32_t prime = fnv_prime();
    std::uint32_t hash = 0x811C9DC5u;
    const auto* p = reinterpret_cast<const unsigned char*>(text);
    for (std::size_t i = 0; i < size; ++i) hash = (hash ^ p[i]) * prime;
    return hash;
}

std::uint32_t table_probe(const PrefabBlock& b, std::uint32_t hash) {
    const std::uint32_t mask = b.table_mask;
    std::uint32_t i = hash & mask;
    for (std::uint16_t v = b.table[i]; v != 0 && b.hashes[v - 1] != hash; v = b.table[i]) i = (i + 1) & mask;
    return i;
}

namespace {

// Inlined into each find (plan 021 T7.6: no call between the hash and the probe).
#if defined(__GNUC__) || defined(__clang__)
__attribute__((always_inline))
#endif
inline Prefab find_hash(const PrefabBlock* b, std::uint32_t hash) {
    if (b == nullptr) return {};
    // The first probe straight-line: a hit or an empty slot ends most finds without entering the loop.
    const std::uint32_t mask = b->table_mask;
    std::uint32_t i = hash & mask;
    std::uint16_t v = b->table[i];
    while (v != 0) {
        if (b->hashes[v - 1] == hash) return prefab_at(*b, v - 1);
        i = (i + 1) & mask;
        v = b->table[i];
    }
    return {};
}

} // namespace
} // namespace detail

Prefab PrefabLibrary::find(PrefabName name) const {
    return detail::find_hash(block_, static_cast<std::uint32_t>(name));
}

Prefab PrefabLibrary::find(const char* name) const {
    if (name == nullptr) return {};
    return detail::find_hash(block_, detail::hash_string(name));
}

Prefab PrefabLibrary::find(const char* text, std::size_t size) const {
    if (text == nullptr) return {};
    return detail::find_hash(block_, detail::hash_bytes(text, size));
}

} // namespace nnh
