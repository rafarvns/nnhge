// The TrueType/OpenType pre-validator (spec 029 T4.6, X): the fixtures pass; every table stb_truetype follows, damaged
// on a copy, is refused before stb_truetype reads it — a truncated directory, tables past the end, a `loca` past
// `glyf`, `cmap` subtables outside, a huge glyph count, a CFF INDEX overflow, CFF2, bad collections.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "test.hpp"
#include "ttf_check.hpp"

using namespace nnh::assetc;

namespace {

const std::string kDir = NNH_FONTS_DIR;

std::vector<unsigned char> load(const char* name) {
    std::vector<unsigned char> out;
    std::FILE* f = std::fopen((kDir + "/" + name).c_str(), "rb");
    if (f == nullptr) return out;
    int c;
    while ((c = std::fgetc(f)) != EOF) out.push_back(static_cast<unsigned char>(c));
    std::fclose(f);
    return out;
}

std::uint32_t be32(const std::vector<unsigned char>& d, std::size_t at) {
    return (std::uint32_t{d[at]} << 24) | (std::uint32_t{d[at + 1]} << 16) | (std::uint32_t{d[at + 2]} << 8) | d[at + 3];
}
void put32(std::vector<unsigned char>& d, std::size_t at, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) d[at + static_cast<std::size_t>(i)] = static_cast<unsigned char>(v >> (24 - 8 * i));
}
void put16(std::vector<unsigned char>& d, std::size_t at, std::uint16_t v) {
    d[at] = static_cast<unsigned char>(v >> 8);
    d[at + 1] = static_cast<unsigned char>(v);
}

/// The directory record of `tag` (offset of the record), or 0.
std::size_t record(const std::vector<unsigned char>& d, const char* tag) {
    const int n = (d[4] << 8) | d[5];
    for (int i = 0; i < n; ++i) {
        const std::size_t r = 12 + static_cast<std::size_t>(i) * 16;
        if (d[r] == tag[0] && d[r + 1] == tag[1] && d[r + 2] == tag[2] && d[r + 3] == tag[3]) return r;
    }
    return 0;
}

std::string check(const std::vector<unsigned char>& d) { return check_ttf(d.data(), d.size(), 0); }

} // namespace

NNH_TEST(ttf_check_accepts_the_fixtures) {
    const auto ttf = load("nnhtest.ttf"), otf = load("nnhtest_cff.otf");
    CHECK(!ttf.empty() && !otf.empty());
    CHECK(check(ttf).empty());
    CHECK(check(otf).empty());
    CHECK(!check_ttf(ttf.data(), ttf.size(), 1).empty()); // one face only
}

NNH_TEST(ttf_check_refuses_a_damaged_directory) {
    const auto good = load("nnhtest.ttf");
    CHECK(!check_ttf(good.data(), 10, 0).empty());   // truncated header
    CHECK(!check_ttf(good.data(), 40, 0).empty());   // truncated directory
    CHECK(!check_ttf(nullptr, 0, 0).empty());
    auto d = good;
    put16(d, 4, 0xFFFF); // numTables past the file
    CHECK(!check(d).empty());
    d = good;
    d[0] = 'X'; // not a font
    CHECK(!check(d).empty());
}

NNH_TEST(ttf_check_refuses_tables_outside_the_file) {
    const auto good = load("nnhtest.ttf");
    for (const char* tag : {"head", "hhea", "maxp", "hmtx", "loca", "glyf", "cmap"}) {
        auto d = good;
        const std::size_t r = record(d, tag);
        CHECK(r != 0);
        if (r == 0) continue;
        put32(d, r + 8, static_cast<std::uint32_t>(d.size() + 16)); // offset past the end
        CHECK(!check(d).empty());
        d = good;
        put32(d, r + 12, 0x7FFFFFF0u); // length past the end
        CHECK(!check(d).empty());
    }
}

NNH_TEST(ttf_check_refuses_inconsistent_glyph_tables) {
    const auto good = load("nnhtest.ttf");
    auto d = good;
    const std::size_t maxp = record(d, "maxp");
    CHECK(maxp != 0);
    if (maxp != 0) {
        put16(d, be32(d, maxp + 8) + 4, 0xFFFF); // numGlyphs: loca and hmtx are too short for it
        CHECK(!check(d).empty());
    }
    d = good;
    const std::size_t loca = record(d, "loca"), glyf = record(d, "glyf");
    CHECK(loca != 0 && glyf != 0);
    if (loca != 0 && glyf != 0) {
        const std::size_t at = be32(d, loca + 8);
        // the second glyph's offset far past glyf (both loca formats: write it as a long and as a short)
        put32(d, at + 4, 0x7FFFFF00u);
        CHECK(!check(d).empty());
    }
    d = good;
    const std::size_t cmap = record(d, "cmap");
    CHECK(cmap != 0);
    if (cmap != 0) {
        const std::size_t at = be32(d, cmap + 8);
        put32(d, at + 4 + 4, 0x00FFFFFFu); // the first subtable's offset outside cmap
        CHECK(!check(d).empty());
    }
}

NNH_TEST(ttf_check_refuses_damaged_cff) {
    const auto good = load("nnhtest_cff.otf");
    auto d = good;
    const std::size_t cff = record(d, "CFF ");
    CHECK(cff != 0);
    if (cff != 0) {
        const std::size_t at = be32(d, cff + 8);
        const unsigned hdr = d[at + 2];              // header size
        put16(d, at + hdr, 0xFFFF);                  // the Name INDEX's count: far more entries than the table holds
        CHECK(!check(d).empty());
        d = good;
        d[cff] = 'C', d[cff + 1] = 'F', d[cff + 2] = 'F', d[cff + 3] = '2'; // variable CFF2: refused
        CHECK(!check(d).empty());
    }
}

NNH_TEST(ttf_check_refuses_bad_collections) {
    std::vector<unsigned char> ttc = {'t', 't', 'c', 'f', 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 20, 0x7F, 0xFF, 0xFF, 0xFF};
    CHECK(!check_ttf(ttc.data(), ttc.size(), 0).empty()); // face 0 points into nothing
    CHECK(!check_ttf(ttc.data(), ttc.size(), 1).empty());
    CHECK(!check_ttf(ttc.data(), ttc.size(), 2).empty()); // no face 2
}
