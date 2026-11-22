// The `.nnh_data` writer (data_index_build.hpp; the format is engine/core/src/data/index_format.hpp).

#include <algorithm>
#include <cstring>

#include "core/crc32.hpp"
#include "data/index_format.hpp"
#include "data/index_validate.hpp"
#include "data_index_build.hpp"

namespace nnh::assetc {

namespace {

namespace fmt = detail::idxfmt;

void put16(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t v) {
    out[at] = static_cast<std::uint8_t>(v);
    out[at + 1] = static_cast<std::uint8_t>(v >> 8);
}

void put32(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t v) {
    put16(out, at, v & 0xFFFF);
    put16(out, at + 2, v >> 16);
}

struct Entry {
    int group;
    const std::string* path;
    std::uint32_t hash;
};

/// Data entries by type then path (the model's order), then textures, sounds and music by path.
std::vector<Entry> entries_of(const IndexModel& m) {
    std::vector<Entry> out;
    for (const IndexModel::Type& t : m.types)
        for (const std::string& p : t.paths)
            out.push_back({0, &p, fnv1a_path(p)});
    for (int g = 0; g < 3; ++g)
        for (const std::string& p : m.media[g])
            out.push_back({g + 1, &p, fnv1a_path(p)});
    return out;
}

} // namespace

std::vector<std::uint8_t> write_index(const IndexModel& m, std::string& problem) {
    const std::vector<Entry> entries = entries_of(m);
    const std::size_t types_at = fmt::kHeaderSize;
    const std::size_t entries_at = types_at + fmt::kTypeSize * m.types.size();
    const std::size_t lookup_at = entries_at + fmt::kEntrySize * entries.size();
    const std::size_t strings_at = lookup_at + fmt::kLookupSize * entries.size();
    std::vector<std::uint8_t> out(strings_at, 0);
    std::memcpy(out.data(), fmt::kMagic, 4);
    put16(out, fmt::kVersionAt, fmt::kVersion);
    put16(out, fmt::kTypeCountAt, static_cast<std::uint32_t>(m.types.size()));
    int counts[fmt::kGroups] = {};
    for (const Entry& e : entries)
        ++counts[e.group];
    for (int g = 0; g < fmt::kGroups; ++g)
        put16(out, fmt::kGroupCountsAt + 2 * static_cast<std::size_t>(g), static_cast<std::uint32_t>(counts[g]));
    put32(out, fmt::kLargestAt, m.largest);
    std::uint32_t first = 0;
    for (std::size_t t = 0; t < m.types.size(); ++t) {
        const std::size_t at = types_at + fmt::kTypeSize * t;
        const std::string& key = m.types[t].key;
        std::memcpy(out.data() + at, key.data(), std::min(key.size(), fmt::kKeyBytes));
        put32(out, at + 16, m.types[t].fingerprint);
        put32(out, at + 20, first);
        put32(out, at + 24, static_cast<std::uint32_t>(m.types[t].paths.size()));
        first += static_cast<std::uint32_t>(m.types[t].paths.size());
    }
    std::vector<std::size_t> lookup;
    for (std::size_t e = 0; e < entries.size(); ++e) {
        const std::size_t at = entries_at + fmt::kEntrySize * e;
        put32(out, at, entries[e].hash);
        put32(out, at + 4, static_cast<std::uint32_t>(out.size() - strings_at));
        out.insert(out.end(), entries[e].path->begin(), entries[e].path->end());
        out.push_back(0);
        lookup.push_back(e);
    }
    std::sort(lookup.begin(), lookup.end(), [&](std::size_t a, std::size_t b) {
        if (entries[a].group != entries[b].group) return entries[a].group < entries[b].group;
        return entries[a].hash < entries[b].hash;
    });
    for (std::size_t i = 0; i < lookup.size(); ++i)
        put16(out, lookup_at + fmt::kLookupSize * i, static_cast<std::uint32_t>(lookup[i]));
    put32(out, fmt::kSizeAt, static_cast<std::uint32_t>(out.size()));
    put32(out, fmt::kCrcAt, detail::crc32(out.data() + fmt::kHeaderSize, out.size() - fmt::kHeaderSize));
    const detail::DataCheck check = detail::validate_index(out.data(), out.size());
    if (!check.ok()) {
        problem = std::string("internal error: the data index fails the engine's check (") + check.reason + ")";
        return {};
    }
    return out;
}

} // namespace nnh::assetc
