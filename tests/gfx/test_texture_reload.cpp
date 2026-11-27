// The texture kind of hot reload (spec 031 T4.1, FR-7, FR-16): the same handle shows the new pixels and size through
// width()/height(); draws use the new platform texture; the old one is destroyed only after the GPU is idle (D47); two
// textures loaded from one path both follow; a bad image refuses the batch and keeps the old texture; a texture that
// wasn't loaded from that path is untouched.

#include <cstring>

#include "../reload/reload_fixture.hpp"
#include "nnh/gfx.hpp"
#include "nnh/texture.hpp"

using namespace reload_test;

namespace {

const unsigned char k16[] = {'F', 'A', 'K', 'E', 16, 0, 16, 0};
const unsigned char k32x64[] = {'F', 'A', 'K', 'E', 32, 0, 64, 0};
const unsigned char kBadSize[] = {'F', 'A', 'K', 'E', 30, 0, 30, 0};

void start_art(char* index, std::size_t capacity) {
    const Put assets[] = {Put{"art/hero.png", k16, sizeof k16}, Put{"art/tree.png", k16, sizeof k16}};
    start(assets, 2, index, capacity);
}

nnh::Texture load(const char* name) {
    auto t = nnh::load_texture(name);
    CHECK(t.ok());
    return t ? std::move(t.value()) : nnh::Texture{};
}

nnh::platform::TextureHandle drawn_handle() {
    const fake::State& s = fake::state();
    return s.last_frame_item_count > 0 ? s.last_frame_items[0].texture : 0;
}

} // namespace

NNH_TEST(texture_reload_the_same_handle_shows_the_new_size_and_pixels) {
    RELOAD_DEV_ONLY();
    char index[128];
    start_art(index, sizeof index);
    {
        nnh::Texture hero = load("art/hero");
        nnh::gfx::draw(nnh::Screen::Top, hero, {0, 0});
        CHECK(nnh::frame());
        const nnh::platform::TextureHandle before = drawn_handle();
        CHECK(hero.width() == 16 && hero.height() == 16);
        const Put batch[] = {Put{"art/hero.png", k32x64, sizeof k32x64}};
        send_batch(1, batch, 1);
        CHECK(nnh::frame()); // applied at this frame's start
        CHECK(nnh::assets::reload_count() == 1);
        CHECK(hero.valid());
        CHECK(hero.width() == 32 && hero.height() == 64);
        CHECK(hero.cached_width() == 16); // the handle itself didn't change
        nnh::gfx::draw(nnh::Screen::Top, hero, {0, 0});
        CHECK(nnh::frame());
        CHECK(drawn_handle() != 0 && drawn_handle() != before);
    }
    nnh::shutdown();
}

NNH_TEST(texture_reload_the_old_texture_waits_for_the_gpu) {
    RELOAD_DEV_ONLY();
    char index[128];
    start_art(index, sizeof index);
    {
        nnh::Texture hero = load("art/hero");
        CHECK(fake::state().live_textures >= 1);
        const int live = fake::state().live_textures;
        const Put batch[] = {Put{"art/hero.png", k32x64, sizeof k32x64}};
        send_batch(1, batch, 1);
        CHECK(nnh::frame());
        CHECK(fake::state().live_textures == live + 1); // the old one is retired, not yet destroyed
        CHECK(nnh::frame());                              // the GPU was idle at this frame's start
        CHECK(fake::state().live_textures == live);
    }
    nnh::shutdown();
}

NNH_TEST(texture_reload_two_textures_from_one_path_follow_and_others_dont) {
    RELOAD_DEV_ONLY();
    char index[128];
    start_art(index, sizeof index);
    {
        nnh::Texture a = load("art/hero");
        nnh::Texture b = load("art/hero");
        nnh::Texture tree = load("art/tree");
        const Put batch[] = {Put{"art/hero.png", k32x64, sizeof k32x64}};
        send_batch(1, batch, 1);
        CHECK(nnh::frame());
        CHECK(a.width() == 32 && b.width() == 32);
        CHECK(tree.width() == 16);
    }
    nnh::shutdown();
}

NNH_TEST(texture_reload_a_bad_image_refuses_and_keeps_the_old) {
    RELOAD_DEV_ONLY();
    char index[128];
    start_art(index, sizeof index);
    {
        nnh::Texture hero = load("art/hero");
        Lines warn("reload refused");
        const Put batch[] = {Put{"art/hero.png", kBadSize, sizeof kBadSize}, text("art/tree.png", "not an image")};
        send_batch(1, batch, 2);
        CHECK(nnh::frame());
        CHECK(nnh::assets::reload_count() == 0);
        CHECK(hero.width() == 16);
        CHECK(warn.count == 1);
        CHECK(std::strstr(warn.last, "art/hero.png: corrupted") != nullptr);
    }
    nnh::shutdown();
}

NNH_TEST(texture_reload_a_released_texture_reads_its_cached_size) {
    RELOAD_DEV_ONLY();
    char index[128];
    start_art(index, sizeof index);
    nnh::Texture hero = load("art/hero");
    nnh::Texture empty;
    CHECK(empty.width() == 0);
    hero.reset();
    CHECK(hero.width() == 0);
    nnh::shutdown();
}
