// The NNHB bindings blob (bindings_format.hpp): encode, sort, validate. Pure; never reads outside `size`.

#include "actions/bindings_format.hpp"

#include "core/crc32.hpp"

namespace nnh::detail::bindings {
namespace {

std::uint32_t le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
           static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}
void put32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}

/// (hash, slot, part) as one sortable number.
std::uint64_t key(const std::uint8_t* e) {
    return static_cast<std::uint64_t>(le32(e)) << 16 | static_cast<std::uint64_t>(e[4]) << 8 | e[5];
}

} // namespace

void encode_entry(const Entry& e, std::uint8_t* out) {
    put32(out, e.hash);
    out[4] = e.slot;
    out[5] = e.part;
    out[6] = e.kind;
    out[7] = e.flags;
    for (int i = 0; i < 4; ++i) out[8 + i] = e.data[i];
}

Entry decode_entry(const std::uint8_t* in) {
    Entry e;
    e.hash = le32(in);
    e.slot = in[4];
    e.part = in[5];
    e.kind = in[6];
    e.flags = in[7];
    for (int i = 0; i < 4; ++i) e.data[i] = in[8 + i];
    return e;
}

void finish_blob(std::uint8_t* blob, int count, std::uint32_t game_version) {
    blob[0] = 'N';
    blob[1] = 'N';
    blob[2] = 'H';
    blob[3] = 'B';
    blob[4] = kFormat;
    blob[5] = 0;
    blob[6] = static_cast<std::uint8_t>(count);
    blob[7] = static_cast<std::uint8_t>(count >> 8);
    put32(blob + 8, game_version);
    put32(blob + 12, crc32(blob + kHeaderSize, kEntrySize * static_cast<std::size_t>(count)));
}

void sort_entries(std::uint8_t* entries, int count) {
    for (int i = 1; i < count; ++i) { // insertion sort: at most 256 entries, saved rarely
        std::uint8_t held[kEntrySize];
        for (std::size_t b = 0; b < kEntrySize; ++b) held[b] = entries[i * kEntrySize + b];
        const std::uint64_t k = key(held);
        int j = i;
        for (; j > 0 && key(entries + (j - 1) * kEntrySize) > k; --j) {
            for (std::size_t b = 0; b < kEntrySize; ++b) entries[j * kEntrySize + b] = entries[(j - 1) * kEntrySize + b];
        }
        for (std::size_t b = 0; b < kEntrySize; ++b) entries[j * kEntrySize + b] = held[b];
    }
}

Check validate(const void* data, std::size_t size, int& count, std::uint32_t& game_version) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    if (p == nullptr || size < kHeaderSize) return Check::Corrupted;
    if (p[0] != 'N' || p[1] != 'N' || p[2] != 'H' || p[3] != 'B') return Check::Corrupted;
    if (p[4] > kFormat) return Check::Unsupported;
    if (p[4] != kFormat || p[5] != 0) return Check::Corrupted;
    const int n = p[6] | p[7] << 8;
    if (n > kMaxEntries || size != kHeaderSize + kEntrySize * static_cast<std::size_t>(n)) return Check::Corrupted;
    if (crc32(p + kHeaderSize, size - kHeaderSize) != le32(p + 12)) return Check::Corrupted;
    count = n;
    game_version = le32(p + 8);
    return Check::Ok;
}

} // namespace nnh::detail::bindings
