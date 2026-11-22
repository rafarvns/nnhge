// Mini-fuzz of `nnh-assetc sprite` (spec 016 T7.4): seeded, deterministic mutations of an export using every feature
// the converter reads, run through the fuzz body (which aborts on a broken invariant). fuzz_assetc_sprite runs the
// same body for longer on Linux.

#include <cstdint>
#include <string>

#include "fuzz_assetc.hpp"
#include "test.hpp"

namespace {

struct Rng {
    std::uint32_t state;
    std::uint32_t next() { return state = state * 1664525u + 1013904223u; }
};

/// 1-6 edits: replace, delete, insert a byte, or insert a JSON-significant character.
void mutate(std::string& s, Rng& rng) {
    static const char kSignificant[] = "{}[]\",:\\-.0123456789@=\n";
    const int edits = 1 + static_cast<int>(rng.next() % 6);
    for (int e = 0; e < edits; ++e) {
        const std::uint32_t r = rng.next();
        const std::size_t at = s.empty() ? 0 : (r >> 8) % s.size();
        switch (r % 4) {
        case 0: if (!s.empty()) s[at] = static_cast<char>(rng.next() >> 16); break;
        case 1: if (!s.empty()) s.erase(at, 1); break;
        case 2: s.insert(at, 1, static_cast<char>(rng.next() >> 16)); break;
        default: s.insert(at, 1, kSignificant[(rng.next() >> 8) % (sizeof kSignificant - 1)]); break;
        }
    }
}

} // namespace

NNH_TEST(assetc_sprite_target_seed_converts) {
    const std::string seed = nnh::assetc::fuzz::sprite_seed();
    CHECK(nnh::assetc::fuzz::sprite_converts(seed.data(), seed.size()));
}

NNH_TEST(assetc_sprite_target_survives_mutated_exports) {
    Rng rng{1601};
    int accepted = 0;
    for (int i = 0; i < 8000; ++i) {
        std::string doc = nnh::assetc::fuzz::sprite_seed();
        mutate(doc, rng);
        nnh::assetc::fuzz::sprite_target(reinterpret_cast<const std::uint8_t*>(doc.data()), doc.size());
        accepted += nnh::assetc::fuzz::sprite_converts(doc.data(), doc.size()) ? 1 : 0;
    }
    CHECK(accepted > 300);  // the deep path (writer + engine validator) runs
    CHECK(accepted < 7600); // and refusals too
}
