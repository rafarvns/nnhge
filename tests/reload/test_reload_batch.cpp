// The batch engine (spec 031 T2.4, FR-4, FR-11, FR-12, FR-16): fake kinds recording calls; one failing prepare abandons
// every prepared item and commits nothing; two kinds on one path; plain files; consistency with and without
// `.nnh_assets`; the index kind's add, remove and rename; an allocation failure at every size leaves everything as it
// was; prepare reads the batch's new bytes while the game still reads the old ones.

#include <cstdio>
#include <cstring>

#include "nnh/assets.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/fs.hpp"
#include "reload/reload_kind.hpp"
#include "reload/reload_memory.hpp"
#include "reload_fixture.hpp"

using namespace reload_test;
using namespace nnh::detail::reload;

namespace {

const char kHero[] = "old hero";
const char kRoom[] = "old room";
char g_log[1024];
const char* g_fail_on = nullptr; // a path whose prepare fails
char g_seen[64];                 // what prepare read through assets:: for that path

void add(const char* what, const ReloadItem& item) {
    char line[160];
    std::snprintf(line, sizeof line, "%s %s;", what, item.path);
    std::strncat(g_log, line, sizeof g_log - std::strlen(g_log) - 1);
}

bool ends_with(const char* s, const char* tail) {
    const std::size_t a = std::strlen(s), b = std::strlen(tail);
    return a >= b && std::strcmp(s + a - b, tail) == 0;
}

bool png_claims(const ReloadItem& item) { return ends_with(item.path, ".png"); }
Prepared png_prepare(const ReloadItem& item) {
    add("prep", item);
    read_text(item.path, g_seen, sizeof g_seen);
    if (g_fail_on != nullptr && std::strcmp(item.path, g_fail_on) == 0) {
        return prepared_refused(wire::ReloadStage::Validate, wire::ReloadReason::Corrupted);
    }
    static int token;
    return prepared_ok(&token);
}
bool g_first_file_at_commit = false; // `.nnh_reload/1.bin` existed when the last commit ran
void png_commit(const ReloadItem& item, void*) {
    add("commit", item);
    g_first_file_at_commit = nnh::platform::fs_size(".nnh_reload/1.bin") >= 0;
}
void png_abandon(const ReloadItem& item, void*) { add("abandon", item); }

bool all_claims(const ReloadItem& item) { return ends_with(item.path, ".png"); }
Prepared all_prepare(const ReloadItem& item) {
    add("prep2", item);
    return prepared_ok();
}
void all_commit(const ReloadItem& item, void*) { add("commit2", item); }
void all_abandon(const ReloadItem& item, void*) { add("abandon2", item); }

ReloadKind g_png{"png", png_claims, png_prepare, png_commit, png_abandon};
ReloadKind g_second{"second", all_claims, all_prepare, all_commit, all_abandon};

void start_game(char* index, std::size_t capacity, bool second = false) {
    const Put assets[] = {text("art/a.png", kHero), text("art/b.png", kHero), text("levels/room.txt", kRoom)};
    start(assets, 3, index, capacity);
    g_log[0] = '\0';
    g_seen[0] = '\0';
    g_fail_on = nullptr;
    register_kind(g_png);
    if (second) register_kind(g_second);
}

} // namespace

NNH_TEST(reload_batch_kinds_prepare_then_commit_in_batch_order) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    const Put batch[] = {text("art/b.png", "B2"), text("levels/room.txt", "R2"), text("art/a.png", "A2")};
    send_batch(1, batch, 3);
    frames(2);
    CHECK(std::strcmp(g_log, "prep art/b.png;prep art/a.png;commit art/b.png;commit art/a.png;") == 0);
    char out[32];
    CHECK(read_text("levels/room.txt", out, sizeof out) == 2); // a plain file: no kind, still reloaded
    CHECK(nnh::assets::reload_count() == 1);
    nnh::shutdown();
}

NNH_TEST(reload_batch_one_failing_prepare_abandons_every_prepared_item) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    g_fail_on = "art/b.png";
    Lines warn("reload refused");
    const Put batch[] = {text("art/a.png", "A2"), text("levels/room.txt", "R2"), text("art/b.png", "B2")};
    send_batch(1, batch, 3);
    frames(2);
    CHECK(std::strcmp(g_log, "prep art/a.png;prep art/b.png;abandon art/a.png;") == 0);
    CHECK(nnh::assets::reload_count() == 0);
    char out[32];
    CHECK(read_text("art/a.png", out, sizeof out) == 8); // nothing of the batch reached the game
    CHECK(read_text("levels/room.txt", out, sizeof out) == 8);
    CHECK(warn.count == 1);
    CHECK(std::strstr(warn.last, "art/b.png: corrupted (old content kept)") != nullptr);
    const Records r = records();
    CHECK(r.refused == 1 && r.last_refused.file == 2 && r.last_refused.reason == wire::ReloadReason::Corrupted);
    nnh::shutdown();
}

NNH_TEST(reload_batch_prepare_reads_the_new_bytes_the_game_still_reads_the_old) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    const Put batch[] = {text("art/a.png", "brand new")};
    send_batch(1, batch, 1);
    frames(2);
    CHECK(std::strcmp(g_seen, "brand new") == 0);
    nnh::shutdown();
}

NNH_TEST(reload_batch_two_kinds_on_one_path) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index, true);
    const Put batch[] = {text("art/a.png", "A2")};
    send_batch(1, batch, 1);
    frames(2);
    CHECK(std::strcmp(g_log, "prep art/a.png;prep2 art/a.png;commit art/a.png;commit2 art/a.png;") == 0);
    nnh::shutdown();
}

NNH_TEST(reload_batch_consistency_without_an_index) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    const Put unknown[] = {text("art/new.png", "N")}; // not in the current index
    send_batch(1, unknown, 1);
    frames(2);
    CHECK(records().last_refused.reason == wire::ReloadReason::Consistency);
    const Put remove[] = {removed("art/a.png")}; // removes need the batch's index
    send_batch(2, remove, 1);
    frames(2);
    CHECK(records().refused == 2);
    CHECK(nnh::assets::exists("art/a.png"));
    CHECK(std::strcmp(g_log, "") == 0); // refused before any kind ran
    nnh::shutdown();
}

NNH_TEST(reload_batch_consistency_with_an_index) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    // a put the new index doesn't list
    const Put bad[] = {text("art/x.png", "X"), text(".nnh_assets", "art/a.png\nart/b.png\nlevels/room.txt\n")};
    send_batch(1, bad, 2);
    frames(2);
    CHECK(records().last_refused.reason == wire::ReloadReason::Consistency);
    // a remove the new index still lists
    const Put bad2[] = {removed("art/a.png"), text(".nnh_assets", "art/a.png\nart/b.png\nlevels/room.txt\n")};
    send_batch(2, bad2, 2);
    frames(2);
    CHECK(records().refused == 2);
    CHECK(nnh::assets::reload_count() == 0);
    nnh::shutdown();
}

NNH_TEST(reload_batch_index_add_remove_rename) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    // rename art/a.png → art/c.png: a remove plus an add
    const Put rename[] = {text("art/c.png", "C"), removed("art/a.png"),
                          text(".nnh_assets", "art/b.png\nart/c.png\nlevels/room.txt\n")};
    send_batch(1, rename, 3);
    frames(1); // applied in this frame: reloaded() answers for it
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(!nnh::assets::exists("art/a.png"));
    CHECK(nnh::assets::exists("art/c.png"));
    CHECK(nnh::assets::exists("art/b.png"));
    CHECK(nnh::assets::reloaded("art/a") && nnh::assets::reloaded("art/c.png"));
    CHECK(std::strcmp(g_log, "prep art/c.png;prep art/a.png;commit art/c.png;commit art/a.png;") == 0);
    // and back: the received art/c.png goes with its path
    const Put back[] = {text("art/a.png", "A3"), removed("art/c.png"),
                        text(".nnh_assets", "art/a.png\nart/b.png\nlevels/room.txt\n")};
    send_batch(2, back, 3);
    frames(2);
    char out[32];
    CHECK(read_text("art/a.png", out, sizeof out) == 2);
    CHECK(!nnh::assets::exists("art/c.png"));
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == -1); // art/c.png's file was removed with its path
    nnh::shutdown();
}

namespace {

/// Batch 2 of a fresh game with `room` heap bytes left; checks that a refusal changed nothing. @return applied.
bool batch_with_room(std::size_t room) {
    char index[256];
    start_game(index, sizeof index);
    const Put warm[] = {text("art/b.png", "W")}; // the first batch allocates the buffer and the overlay
    send_batch(1, warm, 1);
    frames(2);
    const std::size_t used = nnh::mem::region_stats(nnh::MemRegion::Heap).bytes;
    const std::size_t before = nnh::mem::tag_stats(reload_memory_tag()).bytes;
    fake::set_region_capacity(nnh::MemRegion::Heap, used + room);
    const Put batch[] = {text("art/a.png", "A2"), text(".nnh_assets", "art/a.png\nart/b.png\nlevels/room.txt\n")};
    send_batch(2, batch, 2);
    frames(2);
    char out[32];
    const bool applied = nnh::assets::reload_count() == 2;
    if (applied) {
        CHECK(read_text("art/a.png", out, sizeof out) == 2);
    } else {
        CHECK(read_text("art/a.png", out, sizeof out) == 8);
        // what the refused batch allocated went back (its block is freed with it)
        CHECK(nnh::mem::tag_stats(reload_memory_tag()).bytes <= before);
    }
    fake::set_region_capacity(nnh::MemRegion::Heap, 64u << 20);
    nnh::shutdown();
    return applied;
}

} // namespace

NNH_TEST(reload_batch_an_allocation_failure_at_every_size_changes_nothing) {
    RELOAD_DEV_ONLY();
    // The batch's peak need depends on the compiler and the allocator (Linux finding): find the threshold first, then
    // sweep around it, so both outcomes are always exercised.
    CHECK(!batch_with_room(0));
    std::size_t lo = 0, hi = 1u << 20;
    CHECK(batch_with_room(hi));
    while (hi - lo > 64) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (batch_with_room(mid)) hi = mid;
        else lo = mid;
    }
    int refused = 0, applied = 0;
    const std::size_t from = lo > 4000 ? lo - 4000 : 0;
    for (std::size_t room = from; room < hi + 4000; room += 97) (batch_with_room(room) ? applied : refused) += 1;
    CHECK(refused > 0 && applied > 0);
}

NNH_TEST(reload_batch_a_replaced_file_is_removed_after_the_kinds_committed) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    const Put first[] = {text("art/a.png", "A1")}; // received as .nnh_reload/1.bin
    send_batch(1, first, 1);
    frames(2);
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") >= 0);
    const Put second[] = {text("art/a.png", "A2")}; // replaces it
    send_batch(2, second, 1);
    frames(2);
    CHECK(nnh::assets::reload_count() == 2);
    // A kind's commit may still close a stream reading the old file (music): it goes only after (review S8).
    CHECK(g_first_file_at_commit);
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == -1);
    nnh::shutdown();
}
