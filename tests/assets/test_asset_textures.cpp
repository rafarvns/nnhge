// nnh::load_texture() from the game's assets, then from IMAGES (spec 007 FR-4).

#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/texture.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;

namespace {

// The fake platform's image format: "FAKE", width, height (16-bit little endian).
constexpr unsigned char kHero32x16[] = {'F', 'A', 'K', 'E', 32, 0, 16, 0};
constexpr unsigned char kRoot16x8[] = {'F', 'A', 'K', 'E', 16, 0, 8, 0};

void start() {
    nnh::shutdown();
    fake::reset();
    const char index[] = "characters/hero.png\nhero.png\nnotes.txt\n";
    fake::add_asset(".nnh_assets", index, sizeof index - 1);
    fake::add_asset("characters/hero.png", kHero32x16, sizeof kHero32x16);
    fake::add_asset("hero.png", kRoot16x8, sizeof kRoot16x8);
    fake::add_asset("notes.txt", "hello", 5);
    CHECK(nnh::init());
}

} // namespace

NNH_TEST(a_texture_loads_from_an_asset_path_without_extension) {
    start();
    auto t = nnh::load_texture("characters/hero");
    CHECK(t.ok() && t.value().width() == 32 && t.value().height() == 16);
    nnh::shutdown();
}

NNH_TEST(assets_come_before_images_with_the_same_name) {
    start();
    auto t = nnh::load_texture("hero"); // assets/hero.png (16x8), not the test IMAGES "hero" (64x32)
    CHECK(t.ok() && t.value().width() == 16);
    nnh::shutdown();
}

NNH_TEST(images_still_load_when_no_asset_matches) {
    start();
    auto t = nnh::load_texture("tile"); // only in the test IMAGES table
    CHECK(t.ok() && t.value().width() == 16 && t.value().height() == 16);
    nnh::shutdown();
}

NNH_TEST(a_texture_that_is_nowhere_is_not_found) {
    start();
    auto t = nnh::load_texture("characters/villain");
    CHECK(!t.ok() && t.error().code == nnh::ErrorCode::NotFound);
    nnh::shutdown();
}

NNH_TEST(an_asset_that_is_not_an_image_fails_to_decode) {
    start();
    auto t = nnh::load_texture("notes"); // notes.txt is not notes.png: not found as an image
    CHECK(!t.ok() && t.error().code == nnh::ErrorCode::NotFound);
    nnh::shutdown();
}
