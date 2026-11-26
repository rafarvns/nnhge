#pragma once
// Finding a key in the current chain (spec 029 FR-23): for each table of the chain, the bucket of the hash's top bits,
// then the records of that bucket compared. Inline: tr() and format() share it.

#include <bit>

#include "loc/loc_state.hpp"

namespace nnh::detail::loc {

struct Found {
    const LoadedTable* table = nullptr;
    const KeyRecord* key = nullptr;
};

inline bool find_key(std::uint32_t hash, Found& out) {
    const LocState& s = loc_state();
    for (int t = 0; t < s.table_count; ++t) {
        const LoadedTable& table = s.tables[t];
        const std::uint32_t b = bucket_of(hash, table.h.bucket_bits);
        for (std::uint32_t k = table.buckets[b], end = table.buckets[b + 1]; k < end; ++k) {
            if (table.keys[k].hash != hash) continue;
            out = {&table, &table.keys[k]};
            return true;
        }
    }
    return false;
}

/// The string record of a key's form `category` (a plural key without it: its "other"); a plain key: its string.
inline const StringRecord& form_of(const Found& f, int category) {
    const KeyRecord& k = *f.key;
    if (k.flags == 0) return f.table->strings[k.first];
    if (((k.forms >> category) & 1u) == 0) category = Other;
    const int before = std::popcount(static_cast<unsigned>(k.forms & ((1u << category) - 1u)));
    return f.table->strings[k.first + before];
}

inline const char* text_of(const Found& f, const StringRecord& r) { return f.table->text + r.text; }

/// The marker of a key no table has (Debug/Development: its name, logged once; Release: "??").
const char* unknown_key(std::uint32_t hash, const char* name);

} // namespace nnh::detail::loc
