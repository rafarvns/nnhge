// FR-13's queries, the event and the log lines (spec 031 T2.6, FR-13, FR-15, Q5): the counter, reloaded() by path and by
// load name in the batch's frame only, the list, AssetsReloaded delivered in that frame, the exact log lines.

#include <cstring>

#include "nnh/assets.hpp"
#include "nnh/events.hpp"
#include "reload_fixture.hpp"

using namespace reload_test;

namespace {

const char kA[] = "a";

void start_game(char* index, std::size_t capacity) {
    const Put assets[] = {text("art/hero.png", kA), text("levels/level1.nnhmap", kA), text("enemies/goblin.enemy", kA),
                          text("notes.txt", kA)};
    start(assets, 4, index, capacity);
}

} // namespace

NNH_TEST(reload_queries_counter_path_name_and_list_in_the_batch_frame_only) {
    if constexpr (!nnh::assets::kReloadEnabled) return;
    char index[256];
    start_game(index, sizeof index);
    CHECK(nnh::assets::reload_count() == 0);
    CHECK(!nnh::assets::reloaded("art/hero"));
    CHECK(nnh::assets::reloaded_paths().count == 0);
    const Put batch[] = {text("art/hero.png", "x"), text("levels/level1.nnhmap", "y")};
    send_batch(1, batch, 2);
    CHECK(nnh::frame());
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(nnh::assets::reloaded("art/hero.png"));
    CHECK(nnh::assets::reloaded("art/hero"));
    CHECK(nnh::assets::reloaded("levels/level1"));
    CHECK(!nnh::assets::reloaded("enemies/goblin"));
    CHECK(!nnh::assets::reloaded("art"));
    const nnh::assets::ReloadedPaths list = nnh::assets::reloaded_paths();
    CHECK(list.count == 2);
    CHECK(std::strcmp(list.paths[0], "art/hero.png") == 0 && std::strcmp(list.paths[1], "levels/level1.nnhmap") == 0);
    int n = 0;
    for (const char* p : list) n += p != nullptr ? 1 : 0;
    CHECK(n == 2);
    CHECK(nnh::frame()); // the next frame: nothing reloaded in it
    CHECK(!nnh::assets::reloaded("art/hero"));
    CHECK(nnh::assets::reloaded_paths().count == 0);
    CHECK(nnh::assets::reload_count() == 1);
    nnh::shutdown();
}

NNH_TEST(reload_queries_the_event_arrives_in_the_batch_frame) {
    if constexpr (!nnh::assets::kReloadEnabled) return;
    char index[256];
    start_game(index, sizeof index);
    struct Seen {
        int events = 0;
        nnh::AssetsReloaded last;
        std::uint64_t at = 0;
    } seen;
    auto sub = nnh::events::subscribe<nnh::AssetsReloaded>([&seen](const nnh::AssetsReloaded& e) {
        ++seen.events;
        seen.last = e;
        seen.at = nnh::frame_count();
    });
    const Put batch[] = {text("notes.txt", "n"), removed("enemies/goblin.enemy"),
                         text(".nnh_assets", "art/hero.png\nlevels/level1.nnhmap\nnotes.txt\n")};
    send_batch(1, batch, 3);
    CHECK(nnh::frame());
    CHECK(seen.events == 1);
    CHECK(seen.at == nnh::frame_count());
    CHECK(seen.last.count == 1 && seen.last.changed == 1 && seen.last.removed == 1);
    CHECK(sizeof(nnh::AssetsReloaded) == 8);
    nnh::shutdown();
}

NNH_TEST(reload_queries_the_exact_log_lines) {
    if constexpr (!nnh::assets::kReloadEnabled) return;
    char index[256];
    start_game(index, sizeof index);
    Lines info("assets reloaded");
    Lines removed_line("was removed");
    const Put batch[] = {text("art/hero.png", "x"), text("notes.txt", "n"), removed("enemies/goblin.enemy"),
                         text(".nnh_assets", "art/hero.png\nlevels/level1.nnhmap\nnotes.txt\n")};
    send_batch(1, batch, 4);
    CHECK(nnh::frame());
    CHECK(info.count == 1);
    CHECK(std::strstr(info.last, "assets reloaded 4 (frame 2): art/hero, notes, enemies/goblin, .nnh_assets") != nullptr);
    CHECK(removed_line.count == 1);
    CHECK(std::strstr(removed_line.last,
                      "\"enemies/goblin.enemy\" was removed: what was loaded from it keeps its content") != nullptr);
    nnh::shutdown();
}
