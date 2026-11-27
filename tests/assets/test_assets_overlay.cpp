// Asset reads through the hot reload overlay (spec 031 T2.3, FR-5, FR-12): reads, sizes, exists, streams and case hints
// see an applied batch; a removed path is NotFound; an added one loads; the packed file is untouched; a path reloaded
// twice reads its newest file and the older one is removed from the data folder.

#include <cstring>

#include "nnh/assets.hpp"
#include "nnh/platform/fs.hpp"
#include "reload/reload_assets.hpp"
#include "../reload/reload_fixture.hpp"

using namespace reload_test;

namespace {

const char kHero[] = "old hero";
const char kRoom[] = "old room";

void start_game(char* index, std::size_t capacity) {
    const Put assets[] = {text("art/hero.png", kHero), text("levels/room.nnhmap", kRoom)};
    start(assets, 2, index, capacity);
}

} // namespace

NNH_TEST(assets_overlay_a_put_replaces_what_reads_see) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    char out[64];
    CHECK(read_text("art/hero.png", out, sizeof out) == 8);
    const Put batch[] = {text("art/hero.png", "new hero, bigger")};
    send_batch(1, batch, 1);
    frames(2);
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(read_text("art/hero.png", out, sizeof out) == 16);
    CHECK(std::strcmp(out, "new hero, bigger") == 0);
    CHECK(nnh::assets::size("art/hero.png").value_or(0) == 16);
    CHECK(nnh::assets::exists("art/hero.png"));
    auto loaded = nnh::assets::load("art/hero.png");
    CHECK(loaded.ok() && loaded.value().size() == 16);
    CHECK(std::memcmp(kHero, "old hero", 8) == 0); // the packed bytes never change
    CHECK(read_text("levels/room.nnhmap", out, sizeof out) == 8); // other assets as they were
    nnh::shutdown();
}

NNH_TEST(assets_overlay_a_removed_path_is_not_found_and_an_added_one_loads) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    const char* new_index = "art/hero.png\nart/new.png\n";
    const Put batch[] = {text("art/new.png", "fresh"), removed("levels/room.nnhmap"), text(".nnh_assets", new_index)};
    send_batch(1, batch, 3);
    frames(2);
    CHECK(nnh::assets::reload_count() == 1);
    char out[64];
    CHECK(read_text("art/new.png", out, sizeof out) == 5);
    CHECK(!nnh::assets::exists("levels/room.nnhmap"));
    auto gone = nnh::assets::size("levels/room.nnhmap");
    CHECK(!gone.ok() && gone.error().code == nnh::ErrorCode::NotFound);
    CHECK(read_text("art/hero.png", out, sizeof out) == 8); // unchanged, still packed
    nnh::shutdown();
}

NNH_TEST(assets_overlay_a_path_reloaded_twice_reads_its_newest_file) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    const Put one[] = {text("art/hero.png", "first")};
    send_batch(1, one, 1);
    frames(2);
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == 5);
    const Put two[] = {text("art/hero.png", "second!")};
    send_batch(2, two, 1);
    frames(2);
    CHECK(nnh::assets::reload_count() == 2);
    char out[64];
    CHECK(read_text("art/hero.png", out, sizeof out) == 7);
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == -1); // the older received file went
    CHECK(nnh::platform::fs_size(".nnh_reload/2.bin") == 7);
    nnh::shutdown();
    CHECK(nnh::platform::fs_size(".nnh_reload/2.bin") == -1); // and every received file at shutdown
}

NNH_TEST(assets_overlay_streams_read_the_received_file) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    const Put batch[] = {text("levels/room.nnhmap", "streamed room")};
    send_batch(1, batch, 1);
    frames(2);
    nnh::platform::AssetStream stream = 0;
    CHECK(nnh::detail::reload_asset_open("levels/room.nnhmap", stream, true));
    char out[32] = {};
    std::size_t read = 0;
    CHECK(nnh::platform::asset_read_at(stream, 9, out, 4, read) && read == 4);
    CHECK(std::strcmp(out, "room") == 0);
    nnh::platform::asset_close(stream);
    CHECK(fake::state().reload_stream_opens >= 1);
    CHECK(fake::state().open_asset_streams == 0);
    CHECK(nnh::detail::reload_asset_open("art/hero.png", stream, true)); // a packed one: the shared open
    CHECK(fake::state().shared_asset_opens == 1);
    nnh::platform::asset_close(stream);
    nnh::shutdown();
}

NNH_TEST(assets_overlay_case_hints_use_the_current_index) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    const Put batch[] = {text("art/hero.png", "x")};
    send_batch(1, batch, 1);
    frames(2);
    Lines hints("did you mean \"art/hero.png\"");
    CHECK(!nnh::assets::exists("Art/Hero.png"));
    auto r = nnh::assets::size("Art/Hero.png");
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::NotFound);
    CHECK(hints.count == 1);
    nnh::shutdown();
}

NNH_TEST(assets_overlay_without_a_batch_reads_as_before) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    CHECK(!nnh::detail::g_reload_assets_active);
    char out[64];
    CHECK(read_text("art/hero.png", out, sizeof out) == 8);
    CHECK(fake::state().reload_stream_opens == 0);
    nnh::shutdown();
}
