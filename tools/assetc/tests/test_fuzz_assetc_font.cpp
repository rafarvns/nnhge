// Mini-fuzz of the font and string converters (spec 029 T4.10, T5.8): seeded, deterministic mutations of a BMFont, a
// font description, a string file, the TrueType/OpenType fixtures (bytes flipped, tables cut) and a PNG page, run
// through the fuzz bodies (which abort on a broken invariant). The libFuzzer programs run them longer on Linux.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "fuzz_assetc.hpp"
#include "test.hpp"

namespace {

struct Rng {
    std::uint32_t state;
    std::uint32_t next() { return state = state * 1664525u + 1013904223u; }
};

void mutate(std::string& s, Rng& rng, const char* significant) {
    const int edits = 1 + static_cast<int>(rng.next() % 6);
    const std::size_t sig = std::char_traits<char>::length(significant);
    for (int e = 0; e < edits; ++e) {
        const std::uint32_t r = rng.next();
        const std::size_t at = s.empty() ? 0 : (r >> 8) % s.size();
        switch (r % 4) {
        case 0: if (!s.empty()) s[at] = static_cast<char>(rng.next() >> 16); break;
        case 1: if (!s.empty()) s.erase(at, 1); break;
        case 2: s.insert(at, 1, static_cast<char>(rng.next() >> 16)); break;
        default: s.insert(at, 1, significant[(rng.next() >> 8) % sig]); break;
        }
    }
}

template <typename Target>
void run(const std::string& seed, Target target, const char* significant, std::uint32_t seed_rng, int count) {
    Rng rng{seed_rng};
    for (int i = 0; i < count; ++i) {
        std::string doc = seed;
        mutate(doc, rng, significant);
        target(reinterpret_cast<const std::uint8_t*>(doc.data()), doc.size());
    }
}

std::string load(const char* name) {
    std::string out;
    std::FILE* f = std::fopen((std::string(NNH_FONTS_DIR) + "/" + name).c_str(), "rb");
    if (f == nullptr) return out;
    int c;
    while ((c = std::fgetc(f)) != EOF) out.push_back(static_cast<char>(c));
    std::fclose(f);
    return out;
}

} // namespace

NNH_TEST(assetc_font_targets_accept_their_seeds) {
    namespace fz = nnh::assetc::fuzz;
    const std::string s = fz::strings_seed(), b = fz::bmfont_seed(), d = fz::font_desc_seed();
    CHECK(fz::strings_target(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()) == 0);
    CHECK(fz::bmfont_target(reinterpret_cast<const std::uint8_t*>(b.data()), b.size()) == 0);
    CHECK(fz::font_desc_target(reinterpret_cast<const std::uint8_t*>(d.data()), d.size()) == 0);
}

NNH_TEST(assetc_bmfont_and_font_desc_targets_survive_mutations) {
    run(nnh::assetc::fuzz::bmfont_seed(), nnh::assetc::fuzz::bmfont_target, "=\" \n<>/-0123456789", 2901, 6000);
    run(nnh::assetc::fuzz::font_desc_seed(), nnh::assetc::fuzz::font_desc_target, "=\"[],\n#.U+0123456789", 2902, 6000);
}

NNH_TEST(assetc_strings_target_survives_mutations) {
    run(nnh::assetc::fuzz::strings_seed(), nnh::assetc::fuzz::strings_target, "{}[]\"=.\n\\#", 2903, 600); // a clean file evaluates its rule over 10^6 integers
}

NNH_TEST(assetc_ttf_target_survives_damaged_fonts) {
    const std::string ttf = load("nnhtest.ttf"), otf = load("nnhtest_cff.otf");
    CHECK(!ttf.empty() && !otf.empty());
    Rng rng{2904};
    for (int i = 0; i < 400; ++i) {
        std::string f = (i & 1) != 0 ? ttf : otf;
        const int flips = 1 + static_cast<int>(rng.next() % 8);
        for (int k = 0; k < flips; ++k) f[(rng.next() >> 4) % f.size()] = static_cast<char>(rng.next() >> 16);
        if ((rng.next() & 7) == 0) f.resize(f.size() - (rng.next() >> 8) % f.size());
        nnh::assetc::fuzz::ttf_target(reinterpret_cast<const std::uint8_t*>(f.data()), f.size());
    }
}

NNH_TEST(assetc_png_target_survives_damaged_pages) {
    const std::string png = load("mono_0.png");
    CHECK(!png.empty());
    run(png, nnh::assetc::fuzz::png_target, "\x01\xFF\x89PNG", 2905, 3000);
}
