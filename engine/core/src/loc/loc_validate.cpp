// The localization formats' validators (loc_file.hpp): bounded, never allocate; shared with nnh-assetc and fuzzed.

#include <bit>
#include <cstring>

#include "core/crc32.hpp"
#include "loc/loc_file.hpp"
#include "loc/plural.hpp"
#include "system/language_tag.hpp"
#include "text/utf8.hpp"

namespace nnh::detail::loc {
namespace {

bool in(std::uint64_t at, std::uint64_t n, std::uint64_t size) { return at <= size && n <= size - at; }

std::uint32_t crc_without(const std::uint8_t* data, std::size_t size, std::size_t field) {
    const std::uint8_t zero[4] = {};
    std::uint32_t crc = crc32_update(0, data, field);
    crc = crc32_update(crc, zero, 4);
    return crc32_update(crc, data + field + 4, size - field - 4);
}

/// A NUL-terminated string at `at` inside [0, end).
bool c_string(const std::uint8_t* data, std::uint64_t at, std::uint64_t end) {
    return at < end && std::memchr(data + at, 0, static_cast<std::size_t>(end - at)) != nullptr;
}

} // namespace

const char* validate_table(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < sizeof(TableHeader) + kGuardBytes) return "too small";
    TableHeader h;
    std::memcpy(&h, data, sizeof h);
    if (h.magic != kTableMagic) return "not a string table";
    if (h.version != kVersion) return "unsupported version";
    if ((h.flags & ~kFlagNames) != 0 || h.bucket_bits > 15 || h.size != size) return "bad header";
    if (std::memchr(h.code, 0, sizeof h.code) == nullptr) return "bad language code";
    if (h.key_count > 65535 || h.string_count > 65535 || h.placeholder_count > 65535) return "too many keys or strings";
    const std::size_t buckets = (std::size_t{1} << h.bucket_bits) + 1;
    const std::size_t body = size - kGuardBytes;
    // the loader reads the sections through typed pointers: misaligned ones fault on the ARM11 (review S1)
    if (h.off_buckets % 2 != 0 || h.off_keys % 4 != 0 || h.off_strings % 4 != 0 || h.off_placeholders % 4 != 0)
        return "a section isn't aligned";
    if (!in(h.off_buckets, buckets * 2, body) || !in(h.off_keys, std::size_t{h.key_count} * sizeof(KeyRecord), body) ||
        !in(h.off_strings, (std::size_t{h.string_count} + 1) * sizeof(StringRecord), body) ||
        !in(h.off_placeholders, std::size_t{h.placeholder_count} * sizeof(PlaceholderRecord), body) ||
        !in(h.off_text, h.text_bytes, body)) {
        return "a section lies outside the table";
    }
    for (std::size_t i = 0; i < kGuardBytes; ++i)
        if (data[body + i] != 0) return "no zero guard";
    if (crc_without(data, size, kTableCrcOffset) != h.crc) return "bad CRC";
    // buckets never decrease and never pass the key count: tr() walks [buckets[b], buckets[b + 1]) (review B1)
    for (std::size_t b = 0; b + 1 < buckets; ++b) {
        std::uint16_t lo = 0, hi = 0;
        std::memcpy(&lo, data + h.off_buckets + b * 2, 2);
        std::memcpy(&hi, data + h.off_buckets + (b + 1) * 2, 2);
        if (lo > hi || hi > h.key_count) return "buckets don't match the keys";
    }
    std::uint32_t prev = 0;
    std::uint32_t next_string = 0;
    for (std::uint32_t i = 0; i < h.key_count; ++i) {
        KeyRecord k;
        std::memcpy(&k, data + h.off_keys + i * sizeof k, sizeof k);
        if (i > 0 && k.hash <= prev) return "keys not sorted by hash";
        prev = k.hash;
        const std::uint32_t forms = k.flags == 1 ? static_cast<std::uint32_t>(std::popcount(static_cast<unsigned>(k.forms))) : 1;
        if (k.first != next_string || (k.flags == 1 && ((k.forms & (1u << Other)) == 0 || k.forms >= 64)) || k.flags > 1)
            return "a key's strings are out of order";
        next_string += forms;
        // the key is in its bucket's range
        const std::uint32_t b = bucket_of(k.hash, h.bucket_bits);
        std::uint16_t lo = 0, hi = 0;
        std::memcpy(&lo, data + h.off_buckets + b * 2, 2);
        std::memcpy(&hi, data + h.off_buckets + (b + 1) * 2, 2);
        if (i < lo || i >= hi) return "buckets don't match the keys";
    }
    if (next_string != h.string_count) return "string count mismatch";
    std::uint16_t first = 0, last = 0;
    std::memcpy(&first, data + h.off_buckets, 2);
    std::memcpy(&last, data + h.off_buckets + (buckets - 1) * 2, 2);
    if (first != 0 || last != h.key_count) return "buckets don't match the keys";
    for (std::uint32_t i = 0; i < h.string_count; ++i) {
        StringRecord s, n;
        std::memcpy(&s, data + h.off_strings + i * sizeof s, sizeof s);
        std::memcpy(&n, data + h.off_strings + (i + 1) * sizeof n, sizeof n);
        if (!in(s.text, std::size_t{s.length} + 1, h.text_bytes) || data[h.off_text + s.text + s.length] != 0)
            return "a string lies outside the text";
        if (!utf8_valid(reinterpret_cast<const char*>(data + h.off_text + s.text), s.length)) return "a string isn't UTF-8";
        if (n.first_placeholder < s.first_placeholder || n.first_placeholder > h.placeholder_count)
            return "placeholders out of order";
        for (std::uint32_t p = s.first_placeholder; p < n.first_placeholder; ++p) {
            PlaceholderRecord r;
            std::memcpy(&r, data + h.off_placeholders + p * sizeof r, sizeof r);
            if (r.span < 3 || std::size_t{r.at} + r.span > s.length) return "a placeholder lies outside its string";
        }
    }
    if ((h.flags & kFlagNames) != 0) {
        if (!in(h.off_names, std::size_t{h.key_count} * 4, body)) return "names lie outside the table";
        for (std::uint32_t i = 0; i < h.key_count; ++i) {
            std::uint32_t o = 0;
            std::memcpy(&o, data + h.off_names + i * 4, 4);
            if (!c_string(data, std::uint64_t{h.off_names} + o, body)) return "a name lies outside the table";
        }
    }
    return nullptr;
}

const char* validate_manifest(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < sizeof(ManifestHeader)) return "too small";
    ManifestHeader h;
    std::memcpy(&h, data, sizeof h);
    if (h.magic != kManifestMagic) return "not a language manifest";
    if (h.version != kVersion) return "unsupported version";
    if (h.size != size || h.language_count == 0 || h.language_count > kMaxLanguages || h.base >= h.language_count ||
        h.role_count > kMaxRoles || (h.pseudo != kNoIndex && h.pseudo >= h.language_count)) {
        return "bad header";
    }
    if (h.block_bytes > kMaxBlockBytes) return "a bad block size"; // review N1: no wrap when rounded up
    if (h.off_languages % 4 != 0 || h.off_roles % 4 != 0 || h.off_role_fonts % 4 != 0) return "a section isn't aligned";
    const std::size_t n = h.language_count, roles = h.role_count;
    if (!in(h.off_languages, n * sizeof(LanguageRecord), size) || !in(h.off_roles, roles * 4, size) ||
        !in(h.off_role_fonts, n * roles * 4, size) || !in(h.off_blob, h.blob_bytes, size) || h.off_rules > h.off_blob) {
        return "a section lies outside the manifest";
    }
    if (crc_without(data, size, kManifestCrcOffset) != h.crc) return "bad CRC";
    const std::uint64_t blob_end = std::uint64_t{h.off_blob} + h.blob_bytes;
    for (std::size_t i = 0; i < n; ++i) {
        LanguageRecord r;
        std::memcpy(&r, data + h.off_languages + i * sizeof r, sizeof r);
        if (std::memchr(r.code, 0, sizeof r.code) == nullptr || std::memchr(r.grouping, 0, sizeof r.grouping) == nullptr)
            return "a bad language record";
        if (!language_tag_well_formed(r.code)) return "a bad language code"; // review N7
        if (!c_string(data, std::uint64_t{h.off_blob} + r.name, blob_end)) return "a name lies outside the manifest";
        if (r.chain[0] != i) return "a chain doesn't start at its language";
        bool ended = false;
        for (int c = 0; c < kMaxChain; ++c) {
            if (ended) {
                if (r.chain[c] != kNoIndex) return "a bad chain";
                continue;
            }
            if (r.chain[c] == kNoIndex) return "a chain doesn't end at the base";
            if (r.chain[c] >= n) return "a bad chain";
            for (int d = 0; d < c; ++d)
                if (r.chain[d] == r.chain[c]) return "a chain loops";
            ended = r.chain[c] == h.base;
        }
        if (!ended) return "a chain doesn't end at the base";
        if (!in(std::uint64_t{h.off_rules} + r.rule, r.rule_bytes, h.off_blob) ||
            !plural_valid(data + h.off_rules + r.rule, r.rule_bytes))
            return "a bad plural rule";
        if (r.grouping_min < 1 || r.grouping_min > 9) return "a bad grouping";
        for (std::size_t k = 0; k < roles; ++k) {
            std::uint32_t o = 0;
            std::memcpy(&o, data + h.off_role_fonts + (i * roles + k) * 4, 4);
            if (o != 0 && !c_string(data, std::uint64_t{h.off_blob} + o, blob_end)) return "a font path lies outside the manifest";
        }
    }
    return nullptr;
}

} // namespace nnh::detail::loc
