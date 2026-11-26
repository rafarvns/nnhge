// The fake platform's coverage textures (spec 029 FR-2, D49): texture_create_alpha keeps a copy of the coverage bytes
// per handle and counts creations, so core tests can check the pages a font load made.

#include <cstdint>

#include "nnh/platform/video.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;

NNH_TEST(fake_alpha_texture_keeps_its_coverage_bytes) {
    fake::reset();
    std::uint8_t coverage[16 * 8];
    for (int i = 0; i < 16 * 8; ++i) coverage[i] = static_cast<std::uint8_t>((i * 17) & 0xFF);
    nnh::platform::TextureInfo info;
    CHECK(nnh::platform::texture_create_alpha(coverage, 16, 8, info));
    CHECK(fake::state().alpha_textures_created == 1);
    CHECK(fake::state().live_textures == 1);
    int w = 0, h = 0;
    const std::uint8_t* kept = fake::alpha_texture_coverage(info.handle, w, h);
    CHECK(kept != nullptr && w == 16 && h == 8);
    bool same = kept != nullptr;
    for (int i = 0; same && i < 16 * 8; ++i) same = kept[i] == coverage[i];
    CHECK(same);
    coverage[0] = 1; // a copy, not the caller's buffer
    CHECK(kept != nullptr && kept[0] == 0);
    nnh::platform::texture_destroy(info.handle);
    CHECK(fake::alpha_texture_coverage(info.handle, w, h) == nullptr);
    CHECK(fake::state().live_textures == 0);
}

NNH_TEST(fake_alpha_texture_refuses_no_pixels_or_bad_sides) {
    fake::reset();
    std::uint8_t coverage[64] = {};
    nnh::platform::TextureInfo info;
    CHECK(!nnh::platform::texture_create_alpha(nullptr, 8, 8, info));
    CHECK(info.handle == 0);
    CHECK(!nnh::platform::texture_create_alpha(coverage, 0, 8, info));
    CHECK(fake::state().alpha_textures_created == 0);
}
