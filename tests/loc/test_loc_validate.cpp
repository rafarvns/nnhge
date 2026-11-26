// The localization files' validators (spec 029 FR-20, FR-32; loc_validate.cpp): what the writer makes passes; damaged
// headers, sections, records, chains, rules and CRCs are refused with a reason, never read out of bounds.

#include <cstddef>
#include <cstring>
#include <string>

#include "loc_fixture.hpp"
#include "test.hpp"

using namespace loc_test;
namespace lf = nnh::detail::loc;

namespace {

template <typename T>
T get(const std::vector<std::uint8_t>& v, std::size_t at) {
    T x;
    std::memcpy(&x, v.data() + at, sizeof x);
    return x;
}
template <typename T>
void set(std::vector<std::uint8_t>& v, std::size_t at, T x) {
    std::memcpy(v.data() + at, &x, sizeof x);
}

const char* table_why(const std::vector<std::uint8_t>& t) { return lf::validate_table(t.data(), t.size()); }
const char* manifest_why(const std::vector<std::uint8_t>& m) { return lf::validate_manifest(m.data(), m.size()); }

bool says(const char* why, const char* part) { return why != nullptr && std::strstr(why, part) != nullptr; }

} // namespace

NNH_TEST(loc_validate_accepts_what_the_writer_makes) {
    const LocBuilder b = four_languages();
    for (const TestLanguage& l : b.languages) {
        CHECK(table_why(write_table(l)) == nullptr);
        CHECK(table_why(write_table(l, true)) == nullptr);
        CHECK(table_why(write_table(l, false, 0)) == nullptr);
        CHECK(table_why(write_table(l, false, 8)) == nullptr);
    }
    CHECK(manifest_why(b.manifest()) == nullptr);
    TestLanguage empty;
    empty.code = "xx";
    CHECK(table_why(write_table(empty)) == nullptr);
}

NNH_TEST(loc_validate_refuses_damaged_tables) {
    const auto good = write_table(four_languages().languages[0]);
    CHECK(says(lf::validate_table(good.data(), 10), "too small"));
    CHECK(says(lf::validate_table(nullptr, 0), "too small"));
    auto t = good;
    t[0] = 'X';
    CHECK(says(table_why(t), "not a string table"));
    t = good;
    t[100] ^= 1;
    CHECK(says(table_why(t), "bad CRC"));
    t = good;
    t[t.size() - 1] = 1;
    CHECK(says(table_why(t), "guard"));
    t = good;
    set<std::uint32_t>(t, offsetof(lf::TableHeader, size), static_cast<std::uint32_t>(t.size() + 4));
    CHECK(says(table_why(t), "bad header"));
    t = good;
    const auto h = get<lf::TableHeader>(good, 0);
    set<std::uint32_t>(t, offsetof(lf::TableHeader, off_text), static_cast<std::uint32_t>(t.size()));
    seal_table(t);
    CHECK(says(table_why(t), "outside the table"));
    t = write_table(four_languages().languages[0], false, 0); // one bucket; two keys' hashes swapped: not sorted
    const auto h0 = get<lf::TableHeader>(t, 0);
    const auto a = get<std::uint32_t>(t, h0.off_keys), b = get<std::uint32_t>(t, h0.off_keys + 12);
    set(t, h0.off_keys, b);
    set(t, h0.off_keys + 12, a);
    seal_table(t);
    CHECK(says(table_why(t), "sorted"));
    t = good; // a string running past the text
    lf::StringRecord s0 = get<lf::StringRecord>(good, h.off_strings);
    s0.length = static_cast<std::uint16_t>(h.text_bytes);
    set(t, h.off_strings, s0);
    seal_table(t);
    CHECK(says(table_why(t), "outside the text"));
    t = good; // invalid UTF-8
    t[h.off_text] = static_cast<std::uint8_t>(0xFF);
    seal_table(t);
    CHECK(says(table_why(t), "UTF-8"));
}

NNH_TEST(loc_validate_refuses_damaged_placeholders_and_buckets) {
    const auto good = write_table(four_languages().languages[0]);
    const auto h = get<lf::TableHeader>(good, 0);
    auto t = good;
    lf::PlaceholderRecord p = get<lf::PlaceholderRecord>(good, h.off_placeholders);
    p.at = 60000;
    set(t, h.off_placeholders, p);
    seal_table(t);
    CHECK(says(table_why(t), "placeholder"));
    t = good;
    set<std::uint16_t>(t, h.off_buckets, 1);
    seal_table(t);
    CHECK(says(table_why(t), "buckets"));
}

NNH_TEST(loc_validate_refuses_damaged_manifests) {
    const LocBuilder b = four_languages();
    const auto good = b.manifest();
    const auto h = get<lf::ManifestHeader>(good, 0);
    auto m = good;
    m[1] = 'X';
    CHECK(says(manifest_why(m), "not a language manifest"));
    m = good;
    m[m.size() - 1] ^= 0x40;
    CHECK(says(manifest_why(m), "bad CRC"));
    m = good;
    set<std::uint8_t>(m, offsetof(lf::ManifestHeader, base), 9);
    seal_manifest(m);
    CHECK(says(manifest_why(m), "bad header"));
    // a chain that loops, one that doesn't end at the base, one that doesn't start at its language
    lf::LanguageRecord pt = get<lf::LanguageRecord>(good, h.off_languages + 64);
    const std::size_t pt_at = h.off_languages + 64;
    m = good;
    lf::LanguageRecord r = pt;
    r.chain[1] = 1;
    set(m, pt_at, r);
    seal_manifest(m);
    CHECK(says(manifest_why(m), "loops"));
    m = good;
    r = pt;
    r.chain[1] = 2;
    r.chain[2] = lf::kNoIndex;
    set(m, pt_at, r);
    seal_manifest(m);
    CHECK(says(manifest_why(m), "end at the base"));
    m = good;
    r = pt;
    r.chain[0] = 0;
    set(m, pt_at, r);
    seal_manifest(m);
    CHECK(says(manifest_why(m), "start at its language"));
    m = good; // a damaged plural rule
    r = pt;
    r.rule_bytes = 3;
    set(m, pt_at, r);
    seal_manifest(m);
    CHECK(says(manifest_why(m), "plural rule"));
    m = good;
    r = pt;
    r.grouping_min = 0;
    set(m, pt_at, r);
    seal_manifest(m);
    CHECK(says(manifest_why(m), "grouping"));
    m = good;
    r = pt;
    r.name = 100000;
    set(m, pt_at, r);
    seal_manifest(m);
    CHECK(says(manifest_why(m), "name"));
}

NNH_TEST(loc_validate_refuses_buckets_that_decrease_or_pass_the_key_count) {
    // review B1: buckets [0, 50000, 0, 0, 1] with one key in bucket 3 passed, and tr() of a bucket-0 hash then walked
    // 50 000 records past the table
    TestLanguage l;
    l.code = "en";
    std::string name;
    for (int i = 0;; ++i) {
        name = "k" + std::to_string(i);
        if ((nnh::detail::fnv1a32(name.c_str()) >> 30) == 3) break;
    }
    l.key(name, "x");
    auto t = write_table(l, false, 2);
    CHECK(table_why(t) == nullptr);
    const auto h = get<lf::TableHeader>(t, 0);
    const std::uint16_t bad[5] = {0, 50000, 0, 0, 1};
    for (int b = 0; b < 5; ++b) set<std::uint16_t>(t, h.off_buckets + b * 2, bad[b]);
    seal_table(t);
    CHECK(says(table_why(t), "buckets"));
}

NNH_TEST(loc_validate_refuses_misaligned_sections) {
    // review S1: the loader casts the sections to typed pointers (an alignment fault on the ARM11)
    const auto good = write_table(four_languages().languages[0]);
    for (const std::size_t field : {offsetof(lf::TableHeader, off_buckets), offsetof(lf::TableHeader, off_keys),
                                    offsetof(lf::TableHeader, off_strings), offsetof(lf::TableHeader, off_placeholders)}) {
        auto t = good;
        set<std::uint32_t>(t, field, get<std::uint32_t>(good, field) + 1);
        seal_table(t);
        CHECK(says(table_why(t), "aligned"));
    }
    const auto m0 = four_languages().manifest();
    for (const std::size_t field : {offsetof(lf::ManifestHeader, off_languages), offsetof(lf::ManifestHeader, off_roles),
                                    offsetof(lf::ManifestHeader, off_role_fonts)}) {
        auto m = m0;
        set<std::uint32_t>(m, field, get<std::uint32_t>(m0, field) + 2);
        seal_manifest(m);
        CHECK(says(manifest_why(m), "aligned"));
    }
}

NNH_TEST(loc_validate_refuses_an_ill_formed_language_code) {
    // review N7: a manifest's codes are well-formed tags
    const LocBuilder b = four_languages();
    auto m = b.manifest();
    const auto h = get<lf::ManifestHeader>(m, 0);
    lf::LanguageRecord r = get<lf::LanguageRecord>(m, h.off_languages + 64);
    std::memcpy(r.code, "PT_br", 6);
    set(m, h.off_languages + 64, r);
    seal_manifest(m);
    CHECK(says(manifest_why(m), "language code"));
}

NNH_TEST(loc_validate_refuses_a_block_size_that_wraps) {
    // review N1: loc::start() rounds block_bytes up to 8; near 4 GiB that wrapped to a tiny block
    auto m = four_languages().manifest();
    set<std::uint32_t>(m, offsetof(lf::ManifestHeader, block_bytes), 0xFFFFFFF9u);
    seal_manifest(m);
    CHECK(says(manifest_why(m), "block"));
}

NNH_TEST(loc_validate_refuses_buckets_that_decrease_inside_the_keys) {
    // two keys in bucket 3 of 4: [0, 2, 0, 0, 2] stays inside the keys but bucket 1 runs backwards
    TestLanguage l;
    l.code = "en";
    int found = 0;
    for (int i = 0; found < 2; ++i) {
        const std::string name = "k" + std::to_string(i);
        if ((nnh::detail::fnv1a32(name.c_str()) >> 30) != 3) continue;
        l.key(name, "x");
        ++found;
    }
    auto t = write_table(l, false, 2);
    CHECK(table_why(t) == nullptr);
    const auto h = get<lf::TableHeader>(t, 0);
    const std::uint16_t bad[5] = {0, 2, 0, 0, 2};
    for (int b = 0; b < 5; ++b) set<std::uint16_t>(t, h.off_buckets + b * 2, bad[b]);
    seal_table(t);
    CHECK(says(table_why(t), "buckets"));
}
