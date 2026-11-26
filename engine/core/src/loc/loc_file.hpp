#pragma once
// The converted localization formats (spec 029 FR-20, D50; plan "Converted tables"), version 1, little-endian, shared
// by nnh-assetc (which writes them) and the console (loc_validate.cpp reads them). Pure, no engine includes.
//
// `strings/<code>.nnhstr` — one language's table:
//   Header (80 B) · buckets: (2^bits + 1) u16, the first key of each bucket of hash >> (32 − bits) · key records (12 B,
//   sorted by hash) · string records (8 B, one per form; a sentinel at the end) · placeholder records (8 B) · the text
//   (each string NUL-terminated; "{{" stored as "{", "}}" as "}", placeholders kept as written) · in Debug/Development
//   a u32 offset per key into the names that follow (NUL-terminated dotted names) · 16 zero bytes (the guard).
// `.nnh_loc` — the manifest:
//   Header (48 B) · language records (64 B) · role hashes (u32 each) · role font paths (u32 offsets, language × role;
//   0 = none: the chain's) · plural rule bytecode · the text blob (display names, role names, font paths).

#include <cstddef>
#include <cstdint>

namespace nnh::detail::loc {

inline constexpr std::uint32_t kTableMagic = 0x4C484E4Eu;    // "NNHL"
inline constexpr std::uint32_t kManifestMagic = 0x4D484E4Eu; // "NNHM"
inline constexpr std::uint16_t kVersion = 1;
inline constexpr std::uint16_t kFlagNames = 1;
inline constexpr int kMaxLanguages = 32;
inline constexpr int kMaxRoles = 8;
inline constexpr int kMaxChain = 4;
inline constexpr std::size_t kGuardBytes = 16;
inline constexpr std::uint8_t kNoIndex = 0xFF;

// Plural categories, as bits of KeyRecord::forms; a plural key's forms are its string records in this order.
enum Category : std::uint8_t { Zero, One, Two, Few, Many, Other, kCategoryCount };

struct TableHeader {
    std::uint32_t magic = kTableMagic;
    std::uint16_t version = kVersion;
    std::uint16_t flags = 0;
    char code[16] = {};
    std::uint32_t key_count = 0;
    std::uint32_t string_count = 0;      ///< records, the sentinel not counted
    std::uint32_t placeholder_count = 0;
    std::uint32_t text_bytes = 0;
    std::uint8_t bucket_bits = 0;
    std::uint8_t reserved[3] = {};
    std::uint32_t crc = 0;               ///< CRC-32 of the file with this field zero
    std::uint32_t size = 0;              ///< the whole file, guard included
    std::uint32_t off_buckets = 0, off_keys = 0, off_strings = 0, off_placeholders = 0, off_text = 0, off_names = 0;
    std::uint32_t reserved2 = 0;
};
static_assert(sizeof(TableHeader) == 80, "80-byte table header");
inline constexpr std::size_t kTableCrcOffset = 44;

struct KeyRecord {
    std::uint32_t hash = 0;       ///< FNV-1a 32 of the dotted name
    std::uint16_t first = 0;      ///< its first string record
    std::uint8_t forms = 0;       ///< plural keys: bits of the categories it has (1 << Category); 0: one string
    std::uint8_t flags = 0;       ///< 1: plural
    std::uint32_t plural_arg = 0; ///< plural keys: the hash of the placeholder choosing the form
};
static_assert(sizeof(KeyRecord) == 12, "12-byte key records");

struct StringRecord {
    std::uint32_t text = 0;       ///< offset in the text section
    std::uint16_t length = 0;     ///< bytes, the NUL not counted
    std::uint16_t first_placeholder = 0; ///< its placeholders: [first, next record's first)
};
static_assert(sizeof(StringRecord) == 8, "8-byte string records");

struct PlaceholderRecord {
    std::uint32_t name = 0;  ///< FNV-1a 32 of the placeholder's name
    std::uint16_t at = 0;    ///< byte offset of its '{' in the string's text
    std::uint8_t span = 0;   ///< bytes of "{name}"
    std::uint8_t reserved = 0;
};
static_assert(sizeof(PlaceholderRecord) == 8, "8-byte placeholder records");

struct ManifestHeader {
    std::uint32_t magic = kManifestMagic;
    std::uint16_t version = kVersion;
    std::uint16_t flags = 0;
    std::uint8_t language_count = 0;
    std::uint8_t base = 0;
    std::uint8_t pseudo = kNoIndex;
    std::uint8_t role_count = 0;
    std::uint32_t block_bytes = 0; ///< the largest chain's tables together (the run-time block)
    std::uint32_t crc = 0;         ///< CRC-32 of the file with this field zero
    std::uint32_t size = 0;
    std::uint32_t off_languages = 0, off_roles = 0, off_role_fonts = 0, off_rules = 0, off_blob = 0;
    std::uint32_t blob_bytes = 0;
};
static_assert(sizeof(ManifestHeader) == 48, "48-byte manifest header");
inline constexpr std::size_t kManifestCrcOffset = 16;

struct LanguageRecord {
    char code[16] = {};
    std::uint32_t name = 0;              ///< display name: offset in the blob
    std::uint8_t chain[kMaxChain] = {kNoIndex, kNoIndex, kNoIndex, kNoIndex}; ///< itself first, ends at the base
    std::uint32_t rule = 0;              ///< plural bytecode: offset in the rules section
    std::uint16_t rule_bytes = 0;        ///< 0: every integer is "other"
    std::uint8_t grouping_min = 4;       ///< digits from which numbers are grouped (1..9)
    std::uint8_t reserved = 0;
    char grouping[8] = {};               ///< the group separator (UTF-8, NUL-padded; "" = none)
    std::uint32_t table_bytes = 0;       ///< its .nnhstr's size
    std::uint8_t reserved2[20] = {};
};
static_assert(sizeof(LanguageRecord) == 64, "64-byte language records");

/// The bucket of a key hash.
constexpr std::uint32_t bucket_of(std::uint32_t hash, int bits) { return bits == 0 ? 0 : hash >> (32 - bits); }

/// Checks a whole table (bounds, sorted unique hashes, buckets consistent, records and placeholders inside the text,
/// UTF-8, NUL terminators, the guard, the CRC). @return nullptr or the reason.
const char* validate_table(const std::uint8_t* data, std::size_t size);
/// Checks a whole manifest (bounds, chains acyclic and ending at the base, rule bytecode well-formed, blob strings
/// NUL-terminated, the CRC). @return nullptr or the reason.
const char* validate_manifest(const std::uint8_t* data, std::size_t size);

} // namespace nnh::detail::loc
