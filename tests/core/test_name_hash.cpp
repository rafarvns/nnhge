// detail::fnv1a32 (plan 016 D31 note, plan 014 path hashes): the engine's one FNV-1a 32, constexpr, shared
// with nnh-assetc. Vectors from the FNV reference (http://www.isthe.com/chongo/tech/comp/fnv/).

#include "nnh/detail/name_hash.hpp"
#include "test.hpp"

using nnh::detail::fnv1a32;

NNH_TEST(fnv1a32_matches_the_published_vectors) {
    CHECK(fnv1a32("") == 0x811C9DC5u);
    CHECK(fnv1a32("a") == 0xE40C292Cu);
    CHECK(fnv1a32("foobar") == 0xBF9CF968u);
}

NNH_TEST(fnv1a32_of_a_prefix_hashes_only_those_bytes) {
    CHECK(fnv1a32("foobar", 6) == 0xBF9CF968u);
    CHECK(fnv1a32("enemies/goblin.enemy", 14) == fnv1a32("enemies/goblin"));
    CHECK(fnv1a32("abc", 0) == fnv1a32(""));
}

NNH_TEST(fnv1a32_reads_bytes_as_unsigned) {
    CHECK(fnv1a32("\xC3\xA7") == 0x189DDF4Fu); // "ç" in UTF-8
}

NNH_TEST(fnv1a32_is_usable_in_constant_expressions) {
    constexpr std::uint32_t kHero = fnv1a32("hero");
    static_assert(fnv1a32("a") == 0xE40C292Cu);
    switch (fnv1a32("hero")) {
    case kHero:
        CHECK(true);
        break;
    default:
        CHECK(false);
    }
}
