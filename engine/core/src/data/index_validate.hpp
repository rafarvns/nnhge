#pragma once
// Bounded validation of the data index `.nnh_data` (index_format.hpp; spec 014 FR-12, FR-14; constitution X)
// and lookups in a validated one. After validate_index accepts a file, every count is within its limit, every
// section fits the file, every type key is well formed and unique, the types' entries tile the data group,
// every path is a valid asset path whose FNV-1a 32 is its entry's non-zero hash, paths are strictly
// increasing within each type and media group, and the lookup is a permutation sorted by (group, hash) with
// hashes unique per group — so the lookups below never read out of bounds. Pure: nnh-assetc checks the index
// it writes with the same code. Tests: tests/data_assets/test_index_validate.cpp, test_index_lookup.cpp;
// fuzzed by tests/fuzz (data_index_target).

#include <cstddef>
#include <cstdint>

#include "data/data_validate.hpp"
#include "data/index_format.hpp"

namespace nnh::detail {

/// Validates `size` bytes at `data` (any alignment; may be null when size is 0). Problems: Damaged,
/// NewerVersion (a newer version or a flag), OverLimit (larger than idxfmt::kMaxFileBytes). Never allocates;
/// linear in the size, plus types² key comparisons (at most 64²).
DataCheck validate_index(const void* data, std::size_t size);

/// One type of the index.
struct IndexType {
    const char* key = ""; ///< not NUL-terminated when it has 16 characters: use key_length
    int key_length = 0;
    std::uint32_t fingerprint = 0;
    int first = 0; ///< its first entry
    int count = 0;
};

/// Read-only queries on an index validate_index accepted (`data` must stay alive). Never allocate.
class IndexView {
public:
    IndexView() = default;
    explicit IndexView(const void* data) : data_(static_cast<const std::uint8_t*>(data)) {}

    int type_count() const;
    IndexType type(int index) const;
    /// The type whose key is `key` (NUL-terminated), or -1.
    int find_type(const char* key) const;
    /// The type a data entry belongs to, or -1 for a media entry or an index out of range.
    int type_of(int entry) const;

    int entry_count() const;
    int group_first(idxfmt::Group group) const;
    int group_count(idxfmt::Group group) const;
    std::uint32_t hash(int entry) const;
    const char* path(int entry) const;
    /// The entry of `group` whose hash is `hash` (binary search), or -1. 0 is never found.
    int find(idxfmt::Group group, std::uint32_t hash) const;

    /// Bytes of the largest data asset (the loader's scratch buffer).
    std::uint32_t largest() const;

private:
    const std::uint8_t* data_ = nullptr;
};

} // namespace nnh::detail
