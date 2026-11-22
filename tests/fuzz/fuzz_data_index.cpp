// Fuzz target body for the data index `.nnh_data` (spec 014 AC-5; see fuzz_targets.hpp): validate, fix the
// size and CRC so mutations reach the sections, validate again, and run every lookup on an accepted index,
// checking that each one answers consistently.

#include <cstdlib>
#include <cstring>

#include "core/crc32.hpp"
#include "data/index_validate.hpp"
#include "fuzz_targets.hpp"
#include "nnh/detail/name_hash.hpp"

namespace nnh::fuzz {

namespace {

namespace fmt = detail::idxfmt;
using detail::IndexView;

void put16(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
void put32(std::uint8_t* p, std::uint32_t v) {
    put16(p, v);
    put16(p + 2, v >> 16);
}

void fix_header(std::uint8_t* file, std::size_t size) {
    put32(file + fmt::kSizeAt, static_cast<std::uint32_t>(size));
    put32(file + fmt::kCrcAt, detail::crc32(file + fmt::kHeaderSize, size - fmt::kHeaderSize));
}

fmt::Group group_of(const IndexView& v, int entry) {
    fmt::Group g = fmt::Group::Data;
    for (int i = 1; i < fmt::kGroups; ++i) {
        if (entry >= v.group_first(static_cast<fmt::Group>(i))) g = static_cast<fmt::Group>(i);
    }
    return g;
}

/// Every lookup of an accepted index must agree with the others.
void check_lookups(const IndexView& v) {
    char key[fmt::kKeyBytes + 1];
    for (int t = 0; t < v.type_count(); ++t) {
        const detail::IndexType type = v.type(t);
        std::memcpy(key, type.key, static_cast<std::size_t>(type.key_length));
        key[type.key_length] = '\0';
        if (v.find_type(key) != t) std::abort();
        for (int e = type.first; e < type.first + type.count; ++e) {
            if (v.type_of(e) != t) std::abort();
        }
    }
    for (int e = 0; e < v.entry_count(); ++e) {
        const char* path = v.path(e);
        if (v.hash(e) != detail::fnv1a32(path) || std::strlen(path) > 255) std::abort();
        if (v.find(group_of(v, e), v.hash(e)) != e) std::abort();
    }
    if (v.largest() > detail::datafmt::kMaxFileBytes) std::abort();
}

} // namespace

bool data_index_fixed_accepted(const std::uint8_t* data, std::size_t size) {
    static std::uint8_t file[fmt::kMaxFileBytes];
    if (size < fmt::kHeaderSize || size > sizeof file) return false;
    std::memcpy(file, data, size);
    fix_header(file, size);
    return detail::validate_index(file, size).ok();
}

int data_index_target(const std::uint8_t* data, std::size_t size) {
    if (detail::validate_index(data, size).ok()) check_lookups(IndexView{data});
    static std::uint8_t file[fmt::kMaxFileBytes];
    if (size < fmt::kHeaderSize || size > sizeof file) return 0;
    std::memcpy(file, data, size);
    fix_header(file, size);
    if (detail::validate_index(file, size).ok()) check_lookups(IndexView{file});
    return 0;
}

std::size_t data_index_seed(std::uint8_t* out, std::size_t capacity) {
    // Two types (enemy: 2 assets, item: 1), a texture, a sound and a music asset; the lookup sorted by hand
    // below.
    static const char* const kPaths[] = {"enemies/bat", "enemies/goblin", "items/potion",
                                         "art/bat",     "sfx/hit",        "music/theme"};
    constexpr int kEntries = 6;
    static const char* const kKeys[] = {"enemy", "item"};
    const std::uint32_t firsts[] = {0, 2};
    const std::uint32_t counts[] = {2, 1};
    std::size_t size =
        fmt::kHeaderSize + 2 * fmt::kTypeSize + kEntries * (fmt::kEntrySize + fmt::kLookupSize);
    const std::size_t strings_at = size;
    for (const char* p : kPaths)
        size += std::strlen(p) + 1;
    if (capacity < size) return 0;
    std::memset(out, 0, size);
    std::memcpy(out, fmt::kMagic, 4);
    put16(out + fmt::kVersionAt, fmt::kVersion);
    put16(out + fmt::kTypeCountAt, 2);
    const std::uint32_t groups[] = {3, 1, 1, 1};
    for (int g = 0; g < fmt::kGroups; ++g)
        put16(out + fmt::kGroupCountsAt + 2 * g, groups[g]);
    put32(out + fmt::kLargestAt, 200);
    for (int t = 0; t < 2; ++t) {
        std::uint8_t* type = out + fmt::kHeaderSize + fmt::kTypeSize * t;
        std::memcpy(type, kKeys[t], std::strlen(kKeys[t]));
        put32(type + 16, 0x1000u + t);
        put32(type + 20, firsts[t]);
        put32(type + 24, counts[t]);
    }
    const std::size_t entries_at = fmt::kHeaderSize + 2 * fmt::kTypeSize;
    std::size_t at = strings_at;
    int order[kEntries] = {0, 1, 2, 3, 4, 5};
    for (int e = 0; e < kEntries; ++e) {
        put32(out + entries_at + fmt::kEntrySize * e, detail::fnv1a32(kPaths[e]));
        put32(out + entries_at + fmt::kEntrySize * e + 4, static_cast<std::uint32_t>(at - strings_at));
        std::memcpy(out + at, kPaths[e], std::strlen(kPaths[e]) + 1);
        at += std::strlen(kPaths[e]) + 1;
    }
    // Data entries (0-2) sorted by hash; the media groups have one entry each.
    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            if (detail::fnv1a32(kPaths[order[j]]) < detail::fnv1a32(kPaths[order[i]])) {
                const int swap = order[i];
                order[i] = order[j];
                order[j] = swap;
            }
        }
    }
    const std::size_t lookup_at = entries_at + fmt::kEntrySize * kEntries;
    for (int i = 0; i < kEntries; ++i)
        put16(out + lookup_at + fmt::kLookupSize * i, static_cast<std::uint32_t>(order[i]));
    fix_header(out, size);
    return size;
}

} // namespace nnh::fuzz
