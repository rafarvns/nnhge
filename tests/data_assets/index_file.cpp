// Test-only writer of data indexes `NNHX` (data_file.hpp).

#include <algorithm>
#include <cstring>

#include "core/crc32.hpp"
#include "data/index_format.hpp"
#include "data_file.hpp"
#include "nnh/detail/name_hash.hpp"

namespace data_file {

using namespace nnh::detail::idxfmt;

namespace {

struct Entry {
    int group = 0;
    std::string path;
};

std::vector<Entry> entries_of(const IndexSpec& spec) {
    std::vector<Entry> out;
    for (const IndexType& t : spec.types) {
        for (const std::string& p : t.paths)
            out.push_back({0, p});
    }
    for (int g = 0; g < 3; ++g) {
        for (const std::string& p : spec.media[g])
            out.push_back({g + 1, p});
    }
    return out;
}

std::uint32_t hash_of(const std::string& path) {
    return nnh::detail::fnv1a32(path.c_str());
}

} // namespace

std::size_t index_types_at() {
    return kHeaderSize;
}
std::size_t index_entries_at(const IndexSpec& spec) {
    return kHeaderSize + kTypeSize * spec.types.size();
}
std::size_t index_lookup_at(const IndexSpec& spec) {
    return index_entries_at(spec) + kEntrySize * entries_of(spec).size();
}
std::size_t index_strings_at(const IndexSpec& spec) {
    return index_lookup_at(spec) + kLookupSize * entries_of(spec).size();
}

Bytes index_bytes(const IndexSpec& spec) {
    const std::vector<Entry> entries = entries_of(spec);
    Bytes out(index_strings_at(spec), 0);
    std::memcpy(out.data(), kMagic, 4);
    put16(out, kVersionAt, kVersion);
    put16(out, kTypeCountAt, static_cast<std::uint32_t>(spec.types.size()));
    int counts[kGroups] = {};
    for (const Entry& e : entries)
        ++counts[e.group];
    for (int g = 0; g < kGroups; ++g)
        put16(out, kGroupCountsAt + 2 * g, static_cast<std::uint32_t>(counts[g]));
    put32(out, kLargestAt, spec.largest);
    std::uint32_t first = 0;
    for (std::size_t t = 0; t < spec.types.size(); ++t) {
        const std::size_t at = index_types_at() + kTypeSize * t;
        std::memcpy(out.data() + at, spec.types[t].key.data(), std::min(spec.types[t].key.size(), kKeyBytes));
        put32(out, at + 16, spec.types[t].fingerprint);
        put32(out, at + 20, first);
        put32(out, at + 24, static_cast<std::uint32_t>(spec.types[t].paths.size()));
        first += static_cast<std::uint32_t>(spec.types[t].paths.size());
    }
    std::vector<int> lookup;
    for (std::size_t e = 0; e < entries.size(); ++e) {
        const std::size_t at = index_entries_at(spec) + kEntrySize * e;
        put32(out, at, hash_of(entries[e].path));
        put32(out, at + 4, static_cast<std::uint32_t>(out.size() - index_strings_at(spec)));
        out.insert(out.end(), entries[e].path.begin(), entries[e].path.end());
        out.push_back(0);
        lookup.push_back(static_cast<int>(e));
    }
    std::sort(lookup.begin(), lookup.end(), [&](int a, int b) {
        if (entries[a].group != entries[b].group) return entries[a].group < entries[b].group;
        return hash_of(entries[a].path) < hash_of(entries[b].path);
    });
    for (std::size_t i = 0; i < lookup.size(); ++i) {
        put16(out, index_lookup_at(spec) + kLookupSize * i, static_cast<std::uint32_t>(lookup[i]));
    }
    seal_index(out);
    return out;
}

void seal_index(Bytes& file) {
    put32(file, kSizeAt, static_cast<std::uint32_t>(file.size()));
    put32(file, kCrcAt, nnh::detail::crc32(file.data() + kHeaderSize, file.size() - kHeaderSize));
}

} // namespace data_file
