// Validation of the data index and lookups in it (index_validate.hpp). Pure.

#include "data/index_validate.hpp"

#include <cstring>

#include "assets/asset_path.hpp"
#include "core/crc32.hpp"
#include "nnh/detail/name_hash.hpp"

namespace nnh::detail {

namespace {

using namespace idxfmt;

std::uint32_t u16_at(const std::uint8_t* p, std::size_t at) {
    return p[at] | (p[at + 1] << 8);
}
std::uint32_t u32_at(const std::uint8_t* p, std::size_t at) {
    return u16_at(p, at) | (u16_at(p, at + 2) << 16);
}

/// The parts of an index, from its header (sizes only valid once checked).
struct Layout {
    int types = 0;
    int groups[kGroups] = {};
    int entries = 0;
    std::size_t entries_at = 0, lookup_at = 0, strings_at = 0;

    explicit Layout(const std::uint8_t* p) : types(static_cast<int>(u16_at(p, kTypeCountAt))) {
        for (int g = 0; g < kGroups; ++g) {
            groups[g] = static_cast<int>(u16_at(p, kGroupCountsAt + 2 * g));
            entries += groups[g];
        }
        entries_at = kHeaderSize + kTypeSize * static_cast<std::size_t>(types);
        lookup_at = entries_at + kEntrySize * static_cast<std::size_t>(entries);
        strings_at = lookup_at + kLookupSize * static_cast<std::size_t>(entries);
    }
    int group_of(int entry) const {
        int g = 0;
        for (int first = groups[0]; g + 1 < kGroups && entry >= first; first += groups[++g]) {
        }
        return g;
    }
};

DataCheck damaged(const char* reason) {
    return {DataProblem::Damaged, reason};
}

const char* check_header(const std::uint8_t* p, std::size_t size) {
    const Layout l(p);
    if (u16_at(p, kReservedAt) != 0) return "reserved header field isn't 0";
    if (l.types > kMaxTypes) return "too many types";
    if (l.groups[0] > kMaxDataEntries) return "too many data assets";
    if (l.entries - l.groups[0] > kMaxMediaEntries) return "too many media assets";
    if (u32_at(p, kLargestAt) > datafmt::kMaxFileBytes) return "largest data asset over 64 KiB";
    if (l.strings_at > size) return "sections don't fit the file";
    const std::size_t strings = size - l.strings_at;
    if (strings > kMaxStringBytes) return "strings over 256 KiB";
    if ((l.entries == 0) != (strings == 0) || (strings > 0 && p[size - 1] != 0)) {
        return "strings section isn't a list of paths";
    }
    return nullptr;
}

bool key_ok(const std::uint8_t* key) {
    int n = 0;
    while (n < static_cast<int>(kKeyBytes) && key[n] != 0) {
        const char c = static_cast<char>(key[n]);
        const bool letter = c >= 'a' && c <= 'z';
        if (!letter && (n == 0 || !((c >= '0' && c <= '9') || c == '_'))) return false;
        ++n;
    }
    for (int i = n; i < static_cast<int>(kKeyBytes); ++i) {
        if (key[i] != 0) return false;
    }
    return n > 0;
}

const char* check_types(const std::uint8_t* p) {
    const Layout l(p);
    std::uint64_t next = 0;
    for (int t = 0; t < l.types; ++t) {
        const std::uint8_t* type = p + kHeaderSize + kTypeSize * static_cast<std::size_t>(t);
        if (!key_ok(type)) return "bad type key";
        for (int u = 0; u < t; ++u) {
            if (std::memcmp(type, p + kHeaderSize + kTypeSize * static_cast<std::size_t>(u), kKeyBytes) ==
                0) {
                return "type key listed twice";
            }
        }
        if (u32_at(type, 20) != next) return "type entries don't follow each other";
        next += u32_at(type, 24);
    }
    return next == static_cast<std::uint64_t>(l.groups[0]) ? nullptr
                                                           : "type entries don't cover the data assets";
}

/// The path of entry `e` if its offset is in the strings (which end with a NUL), else null.
const char* entry_path(const std::uint8_t* p, std::size_t size, const Layout& l, int e) {
    const std::uint32_t offset = u32_at(p, l.entries_at + kEntrySize * static_cast<std::size_t>(e) + 4);
    if (offset >= size - l.strings_at) return nullptr;
    return reinterpret_cast<const char*>(p + l.strings_at + offset);
}

/// Where the sorted runs of paths start: each type's entries, then each media group.
bool starts_run(const std::uint8_t* p, const Layout& l, int e) {
    if (e >= l.groups[0]) return l.group_of(e) != l.group_of(e - 1);
    for (int t = 0; t < l.types; ++t) {
        if (u32_at(p, kHeaderSize + kTypeSize * static_cast<std::size_t>(t) + 20) ==
            static_cast<std::uint32_t>(e)) {
            return true;
        }
    }
    return false;
}

const char* check_entries(const std::uint8_t* p, std::size_t size) {
    const Layout l(p);
    const char* previous = nullptr;
    for (int e = 0; e < l.entries; ++e) {
        const char* path = entry_path(p, size, l, e);
        if (path == nullptr || !valid_asset_path(path)) return "bad path";
        const std::uint32_t hash = u32_at(p, l.entries_at + kEntrySize * static_cast<std::size_t>(e));
        if (hash == 0 || hash != fnv1a32(path)) return "path hash doesn't match its path";
        if (e > 0 && !starts_run(p, l, e) && std::strcmp(previous, path) >= 0) return "paths out of order";
        previous = path;
    }
    return nullptr;
}

const char* check_lookup(const std::uint8_t* p, const Layout& l) {
    std::uint64_t previous = 0;
    for (int i = 0; i < l.entries; ++i) {
        const std::uint32_t e = u16_at(p, l.lookup_at + kLookupSize * static_cast<std::size_t>(i));
        if (e >= static_cast<std::uint32_t>(l.entries)) return "lookup entry out of range";
        const std::uint64_t order = (static_cast<std::uint64_t>(l.group_of(static_cast<int>(e))) << 32) |
                                    u32_at(p, l.entries_at + kEntrySize * e);
        if (i > 0 && order <= previous) return "lookup out of order (or a hash twice in a group)";
        previous = order;
    }
    return nullptr;
}

} // namespace

DataCheck validate_index(const void* data, std::size_t size) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    if (size < kHeaderSize) return damaged("too short to be a data index");
    const std::uint32_t version = u16_at(p, kVersionAt);
    if (std::memcmp(p, kMagic, 4) != 0 || version == 0) return damaged("not a data index");
    if (version > kVersion) return {DataProblem::NewerVersion, "data index written by a newer NNHGE"};
    if (u16_at(p, kFlagsAt) != 0)
        return {DataProblem::NewerVersion, "data index uses a feature this engine lacks"};
    if (u32_at(p, kSizeAt) != size) return damaged("data index size doesn't match the header");
    if (size > kMaxFileBytes) return {DataProblem::OverLimit, "data index larger than its limit"};
    if (u32_at(p, kCrcAt) != crc32(p + kHeaderSize, size - kHeaderSize))
        return damaged("data index CRC mismatch");
    const char* reason = check_header(p, size);
    if (reason == nullptr) reason = check_types(p);
    if (reason == nullptr) reason = check_entries(p, size);
    if (reason == nullptr) reason = check_lookup(p, Layout(p));
    return reason == nullptr ? DataCheck{} : damaged(reason);
}

int IndexView::type_count() const {
    return static_cast<int>(u16_at(data_, kTypeCountAt));
}

IndexType IndexView::type(int index) const {
    const std::uint8_t* t = data_ + kHeaderSize + kTypeSize * static_cast<std::size_t>(index);
    IndexType out;
    out.key = reinterpret_cast<const char*>(t);
    while (out.key_length < static_cast<int>(kKeyBytes) && t[out.key_length] != 0)
        ++out.key_length;
    out.fingerprint = u32_at(t, 16);
    out.first = static_cast<int>(u32_at(t, 20));
    out.count = static_cast<int>(u32_at(t, 24));
    return out;
}

int IndexView::find_type(const char* key) const {
    const std::size_t length = std::strlen(key);
    for (int i = 0; i < type_count(); ++i) {
        const IndexType t = type(i);
        if (static_cast<std::size_t>(t.key_length) == length && std::memcmp(t.key, key, length) == 0)
            return i;
    }
    return -1;
}

int IndexView::type_of(int entry) const {
    if (entry < 0 || entry >= group_count(idxfmt::Group::Data)) return -1;
    for (int i = 0; i < type_count(); ++i) {
        const IndexType t = type(i);
        if (entry >= t.first && entry < t.first + t.count) return i;
    }
    return -1;
}

int IndexView::entry_count() const {
    return Layout(data_).entries;
}

int IndexView::group_first(idxfmt::Group group) const {
    const Layout l(data_);
    int first = 0;
    for (int g = 0; g < static_cast<int>(group); ++g)
        first += l.groups[g];
    return first;
}

int IndexView::group_count(idxfmt::Group group) const {
    return Layout(data_).groups[static_cast<int>(group)];
}

std::uint32_t IndexView::hash(int entry) const {
    return u32_at(data_, Layout(data_).entries_at + kEntrySize * static_cast<std::size_t>(entry));
}

const char* IndexView::path(int entry) const {
    const Layout l(data_);
    const std::uint32_t offset =
        u32_at(data_, l.entries_at + kEntrySize * static_cast<std::size_t>(entry) + 4);
    return reinterpret_cast<const char*>(data_ + l.strings_at + offset);
}

int IndexView::find(idxfmt::Group group, std::uint32_t hash) const {
    const Layout l(data_);
    const std::uint64_t want = (static_cast<std::uint64_t>(group) << 32) | hash;
    int low = 0;
    int high = l.entries;
    while (low < high) {
        const int mid = low + (high - low) / 2;
        const std::uint32_t e = u16_at(data_, l.lookup_at + kLookupSize * static_cast<std::size_t>(mid));
        const std::uint64_t have = (static_cast<std::uint64_t>(l.group_of(static_cast<int>(e))) << 32) |
                                   this->hash(static_cast<int>(e));
        if (have == want) return static_cast<int>(e);
        if (have < want)
            low = mid + 1;
        else
            high = mid;
    }
    return -1;
}

std::uint32_t IndexView::largest() const {
    return u32_at(data_, kLargestAt);
}

} // namespace nnh::detail
