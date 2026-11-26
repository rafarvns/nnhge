// The tilemap kind of hot reload (spec 031 T2.7, FR-11, FR-18, Q6): a valid reloaded map changes nothing the game holds
// (it is announced; the game loads it again), a corrupted one refuses the whole batch.

#include "../reload/reload_fixture.hpp"
#include "load_fixture.hpp"

using namespace reload_test;

namespace {

void start_maps() {
    load_test::start();
    fake::remote_connect();
    send(remote_test::hello());
    CHECK(nnh::frame());
}

} // namespace

NNH_TEST(map_reload_a_valid_map_is_announced_and_the_held_map_unchanged) {
    RELOAD_DEV_ONLY();
    start_maps();
    {
        nnh::Tilemap held = load_test::load("levels/one");
        CHECK(held.object_count() == 2);
        static map_file::Bytes moved;
        moved = map_file::sample("tiles/world");
        const Put batch[] = {Put{"levels/one.nnhmap", moved.data(), moved.size()}};
        send_batch(1, batch, 1);
        CHECK(nnh::frame());
        CHECK(nnh::assets::reloaded("levels/one"));
        CHECK(held.object_count() == 2 && held.width() == 8); // the game's copy, as it was
        nnh::Tilemap again = load_test::load("levels/one");     // the game's reaction: load it again
        CHECK(again.width() == 8);
    }
    nnh::shutdown();
}

NNH_TEST(map_reload_a_corrupted_map_refuses_the_batch) {
    RELOAD_DEV_ONLY();
    start_maps();
    {
        nnh::Tilemap held = load_test::load("levels/one"); // registers the kind
        static map_file::Bytes broken;
        broken = map_file::sample("tiles/world");
        broken[broken.size() / 2] ^= 0x5A; // the CRC no longer matches
        Lines warn("reload refused");
        const Put batch[] = {text("tiles/note.txt", "x"), Put{"levels/one.nnhmap", broken.data(), broken.size()}};
        static const char assets[] = "levels/one.nnhmap\nlevels/other.nnhmap\ntiles/note.txt\ntiles/small.png\n"
                                     "tiles/tall.png\ntiles/world.png\n";
        const Put with_index[] = {batch[0], batch[1], Put{".nnh_assets", assets, sizeof assets - 1}};
        send_batch(1, with_index, 3);
        CHECK(nnh::frame());
        CHECK(nnh::assets::reload_count() == 0);
        CHECK(!nnh::assets::exists("tiles/note.txt")); // nothing of the batch applied
        CHECK(warn.count == 1);
        CHECK(std::strstr(warn.last, "levels/one.nnhmap: corrupted (old content kept)") != nullptr);
        CHECK(load_test::load_error("levels/one") == nnh::ErrorCode::None); // the packed map still loads
    }
    nnh::shutdown();
}
