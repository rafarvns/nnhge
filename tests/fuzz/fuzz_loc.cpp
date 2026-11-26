// Fuzz targets of the localization validators (spec 029 FR-20, FR-32): see fuzz_targets.hpp. Mutations are re-sealed
// (size and CRC fixed) so they reach the deep checks; an accepted file is then walked the way the console reads it.

#include <bit>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/crc32.hpp"
#include "fuzz_targets.hpp"
#include "loc/loc_file.hpp"
#include "loc/plural.hpp"

namespace nnh::fuzz {
namespace {

namespace lf = detail::loc;

std::vector<std::uint8_t> sealed(const std::uint8_t* data, std::size_t size, std::size_t size_at, std::size_t crc_at) {
    std::vector<std::uint8_t> f(data, data + size);
    if (size < crc_at + 4 || size < size_at + 4) return f;
    const auto n = static_cast<std::uint32_t>(size);
    std::memcpy(f.data() + size_at, &n, 4);
    std::memset(f.data() + crc_at, 0, 4);
    const std::uint32_t crc = detail::crc32(f.data(), f.size());
    std::memcpy(f.data() + crc_at, &crc, 4);
    return f;
}

/// Every lookup of an accepted table stays inside it (tr, format's records and placeholders).
void walk_table(const std::uint8_t* data, std::size_t size) {
    lf::TableHeader h;
    std::memcpy(&h, data, sizeof h);
    for (std::uint32_t k = 0; k < h.key_count; ++k) {
        lf::KeyRecord key;
        std::memcpy(&key, data + h.off_keys + k * sizeof key, sizeof key);
        const int forms = key.flags == 1 ? std::popcount(static_cast<unsigned>(key.forms)) : 1;
        for (int f = 0; f < forms; ++f) {
            lf::StringRecord s, next;
            std::memcpy(&s, data + h.off_strings + (key.first + f) * sizeof s, sizeof s);
            std::memcpy(&next, data + h.off_strings + (key.first + f + 1) * sizeof s, sizeof s);
            if (h.off_text + s.text + s.length >= size) std::abort();
            for (std::uint32_t p = s.first_placeholder; p < next.first_placeholder; ++p) {
                lf::PlaceholderRecord r;
                std::memcpy(&r, data + h.off_placeholders + p * sizeof r, sizeof r);
                if (r.at + r.span > s.length) std::abort();
            }
        }
    }
}

/// Every read of an accepted manifest stays inside it (chains, rules, names, font paths).
void walk_manifest(const std::uint8_t* data, std::size_t size) {
    lf::ManifestHeader h;
    std::memcpy(&h, data, sizeof h);
    for (int i = 0; i < h.language_count; ++i) {
        lf::LanguageRecord r;
        std::memcpy(&r, data + h.off_languages + i * sizeof r, sizeof r);
        if (std::size_t{h.off_blob} + r.name >= size) std::abort();
        for (std::uint64_t n : {0ull, 1ull, 2ull, 11ull, 101ull, 1000001ull})
            (void)lf::plural_category(data + h.off_rules + r.rule, r.rule_bytes, n);
        for (int c = 0; c < lf::kMaxChain; ++c)
            if (r.chain[c] != lf::kNoIndex && r.chain[c] >= h.language_count) std::abort();
    }
}

} // namespace

int loc_table_target(const std::uint8_t* data, std::size_t size) {
    if (lf::validate_table(data, size) == nullptr) walk_table(data, size);
    const auto fixed = sealed(data, size, 48, lf::kTableCrcOffset);
    if (lf::validate_table(fixed.data(), fixed.size()) == nullptr) walk_table(fixed.data(), fixed.size());
    return 0;
}

int loc_manifest_target(const std::uint8_t* data, std::size_t size) {
    if (lf::validate_manifest(data, size) == nullptr) walk_manifest(data, size);
    const auto fixed = sealed(data, size, 20, lf::kManifestCrcOffset);
    if (lf::validate_manifest(fixed.data(), fixed.size()) == nullptr) walk_manifest(fixed.data(), fixed.size());
    return 0;
}

bool loc_table_fixed_accepted(const std::uint8_t* data, std::size_t size) {
    const auto fixed = sealed(data, size, 48, lf::kTableCrcOffset);
    return lf::validate_table(fixed.data(), fixed.size()) == nullptr;
}

bool loc_manifest_fixed_accepted(const std::uint8_t* data, std::size_t size) {
    const auto fixed = sealed(data, size, 20, lf::kManifestCrcOffset);
    return lf::validate_manifest(fixed.data(), fixed.size()) == nullptr;
}

} // namespace nnh::fuzz
