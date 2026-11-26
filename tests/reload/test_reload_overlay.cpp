// The overlay table (spec 031 T1.5, FR-5, FR-12, FR-16): insert, replace, remove, forced collisions, capacity + 1,
// repointing to a new index's paths.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "reload/reload_overlay.hpp"
#include "test.hpp"

using namespace nnh::detail::reload;

namespace {

struct Table {
    OverlayEntry slots[kOverlaySlots];
    OverlayTable t;
    Table() { t.attach(slots); }
};

} // namespace

NNH_TEST(reload_overlay_path_hash_is_fnv1a) {
    CHECK(path_hash("") == 0x811C9DC5u);
    CHECK(path_hash("a") == 0xE40C292Cu);
    CHECK(path_hash("art/hero.png") != path_hash("art/hero.pn"));
}

NNH_TEST(reload_overlay_put_find_replace_remove) {
    Table tb;
    const char* path = "art/hero.png";
    std::uint16_t replaced = 99;
    CHECK(tb.t.find(path, path_hash(path)) == nullptr);
    CHECK(tb.t.put(path, path_hash(path), 1, 100, replaced));
    CHECK(replaced == 0);
    const OverlayEntry* e = tb.t.find("art/hero.png", path_hash(path)); // another pointer, same text
    CHECK(e != nullptr);
    CHECK(e->file == 1 && e->size == 100);
    CHECK(tb.t.put(path, path_hash(path), 2, 200, replaced));
    CHECK(replaced == 1);
    CHECK(tb.t.find(path, path_hash(path))->file == 2);
    CHECK(tb.t.paths() == 1);
    CHECK(tb.t.remove(path, path_hash(path)) == 2);
    CHECK(tb.t.find(path, path_hash(path)) == nullptr);
    CHECK(tb.t.remove(path, path_hash(path)) == 0);
    CHECK(tb.t.put(path, path_hash(path), 3, 1, replaced)); // added again: a new slot
    CHECK(replaced == 0);
    CHECK(tb.t.paths() == 2);
}

NNH_TEST(reload_overlay_collisions_compare_exactly) {
    Table tb;
    std::uint16_t replaced = 0;
    CHECK(tb.t.put("a.png", 7, 1, 1, replaced));
    CHECK(tb.t.put("b.png", 7, 2, 2, replaced)); // the same hash, forced
    CHECK(replaced == 0);
    CHECK(tb.t.find("a.png", 7)->file == 1);
    CHECK(tb.t.find("b.png", 7)->file == 2);
    CHECK(tb.t.find("c.png", 7) == nullptr);
    CHECK(tb.t.remove("a.png", 7) == 1);
    CHECK(tb.t.find("b.png", 7)->file == 2); // the probe goes past a tombstone
    // the last slot wraps to the first
    CHECK(tb.t.put("w1", kOverlaySlots - 1, 3, 1, replaced));
    CHECK(tb.t.put("w2", kOverlaySlots - 1, 4, 1, replaced));
    CHECK(tb.t.find("w2", kOverlaySlots - 1)->file == 4);
}

NNH_TEST(reload_overlay_capacity_plus_one_is_refused) {
    Table tb;
    static char names[kOverlayPaths + 1][12];
    std::uint16_t replaced = 0;
    for (int i = 0; i < kOverlayPaths; ++i) {
        std::snprintf(names[i], sizeof names[i], "f%d", i);
        CHECK(tb.t.put(names[i], path_hash(names[i]), static_cast<std::uint16_t>(i + 1), 1, replaced));
    }
    std::snprintf(names[kOverlayPaths], sizeof names[0], "f%d", kOverlayPaths);
    CHECK(!tb.t.put(names[kOverlayPaths], path_hash(names[kOverlayPaths]), 999, 1, replaced));
    CHECK(tb.t.put(names[3], path_hash(names[3]), 1000, 1, replaced)); // a live path is replaced in its slot
    CHECK(replaced == 4);
    CHECK(tb.t.paths() == kOverlayPaths);
}

namespace {

struct Repoint {
    const char* new_paths[2] = {"x/keep.png", nullptr};
    int removed = 0;
    std::uint16_t removed_file = 0;
};

const char* resolve(void* c, const char* old) {
    auto* r = static_cast<Repoint*>(c);
    return std::strcmp(old, r->new_paths[0]) == 0 ? r->new_paths[0] : nullptr;
}

void on_removed(void* c, std::uint16_t file) {
    auto* r = static_cast<Repoint*>(c);
    ++r->removed;
    r->removed_file = file;
}

void count_file(void* c, std::uint16_t file) { *static_cast<int*>(c) += file; }

} // namespace

NNH_TEST(reload_overlay_repoint_follows_a_new_index) {
    Table tb;
    char old_keep[] = "x/keep.png";
    char old_gone[] = "x/gone.png";
    std::uint16_t replaced = 0;
    tb.t.put(old_keep, path_hash(old_keep), 5, 1, replaced);
    tb.t.put(old_gone, path_hash(old_gone), 6, 1, replaced);
    Repoint r;
    tb.t.repoint(resolve, &r, on_removed);
    CHECK(r.removed == 1);
    CHECK(r.removed_file == 6);
    const OverlayEntry* e = tb.t.find("x/keep.png", path_hash("x/keep.png"));
    CHECK(e != nullptr);
    CHECK(e->path == r.new_paths[0]); // now in the new index
    std::memset(old_keep, 0, sizeof old_keep); // the old index's text is gone
    CHECK(tb.t.find("x/keep.png", path_hash("x/keep.png")) != nullptr);
    int sum = 0;
    tb.t.each_file(count_file, &sum);
    CHECK(sum == 5);
}

NNH_TEST(reload_overlay_attach_forgets_everything) {
    Table tb;
    std::uint16_t replaced = 0;
    tb.t.put("a", path_hash("a"), 1, 1, replaced);
    tb.t.attach(tb.slots);
    CHECK(tb.t.find("a", path_hash("a")) == nullptr);
    CHECK(tb.t.paths() == 0);
}
