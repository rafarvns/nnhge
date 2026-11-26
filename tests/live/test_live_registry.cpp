// The registry (spec 030 T2.1, FR-1, FR-6, FR-18, FR-19): block size, tag accounting, sorted order under random
// inserts and removals, generations, asserts, capacity + 1, configure() after use, the block freed at shutdown, the ops
// pointer null until the first registration.

#include <cstdio>
#include <cstring>

#include "debug/live_hook.hpp"
#include "live/live_registry.hpp"
#include "live_fixture.hpp"
#include "nnh/memory.hpp"

using namespace nnh::detail::live;
using live_test::asserts_or_is_ignored;
using live_test::restart;

namespace {

float g_values[200];

int name_of(char* out, int i) { return std::snprintf(out, 32, "g%d/v%03d", i % 7, i); }

bool index_sorted(const Registry& r) {
    for (int i = 1; i < r.tunable_count; ++i) {
        const Entry& a = r.entries[r.tunable_index[i - 1]];
        const Entry& b = r.entries[r.tunable_index[i]];
        const int n = a.name_length < b.name_length ? a.name_length : b.name_length;
        const int c = std::memcmp(a.name, b.name, static_cast<std::size_t>(n));
        if (c > 0 || (c == 0 && a.name_length >= b.name_length)) return false;
    }
    return true;
}

} // namespace

NNH_TEST(live_registry_block_size_follows_the_capacities) {
    const std::size_t base = block_bytes(0, 0, 0);
    CHECK(block_bytes(1, 0, 0) - base >= sizeof(Entry) + 2);
    CHECK(block_bytes(128, 64, 64) - base == ((sizeof(Entry) * 128 + 7) & ~std::size_t{7}) +
                                                 ((sizeof(WatchEntry) * 64 + 7) & ~std::size_t{7}) +
                                                 ((sizeof(ShownEntry) * 64 + 7) & ~std::size_t{7}) + 256 + 256 + 256);
    if constexpr (sizeof(void*) == 4) { // the plan's 3DS sizes
        CHECK(sizeof(Entry) <= 84);
        CHECK(sizeof(WatchEntry) == 48);
    }
    CHECK(sizeof(ShownEntry) <= 80);
    CHECK(sizeof(RemoteOp) == 12);
    CHECK(sizeof(Follow) == 12);
}

NNH_TEST(live_registry_ops_pointer_is_null_until_the_first_registration) {
    restart();
    CHECK(nnh::detail::g_live_ops == nullptr);
    CHECK(g_registry == nullptr);
    if constexpr (!nnh::live::kEnabled) return;
    float v = 1;
    auto h = nnh::live::tune("a/b", &v, {.min = 0, .max = 2, .step = 0.5f});
    CHECK(nnh::detail::g_live_ops != nullptr);
    CHECK(g_registry != nullptr);
}

NNH_TEST(live_registry_block_is_tagged_and_freed_at_shutdown) {
    if constexpr (!nnh::live::kEnabled) return; // Release runs no shutdown hooks (and games link no registry)
    restart();
    const int before = nnh::mem::live_allocations();
    Registry& r = registry();
    CHECK(nnh::mem::live_allocations() == before + 1);
    CHECK(r.max_tunables == 128 && r.max_watches == 64 && r.max_shown == 64);
    nnh::shutdown();
    CHECK(g_registry == nullptr);
    CHECK(nnh::detail::g_live_ops == nullptr);
    live_test::fake::reset();
    CHECK(nnh::init({}));
}

NNH_TEST(live_registry_keeps_the_index_sorted_under_random_inserts_and_removals) {
    if constexpr (!nnh::live::kEnabled) return;
    restart();
    Registry& r = registry();
    int slots[200];
    bool used[200] = {};
    std::uint32_t state = 77;
    for (int step = 0; step < 2000; ++step) {
        state = state * 1664525u + 1013904223u;
        const int i = static_cast<int>((state >> 8) % 120);
        if (used[i]) {
            remove_tunable(r, slots[i]);
            used[i] = false;
        } else {
            char name[32];
            const int n = name_of(name, i);
            CHECK(name_usable(r, name, n));
            slots[i] = add_tunable(r, name, n, nnh::detail::fnv1a32(name, static_cast<std::size_t>(n)));
            CHECK(slots[i] >= 0);
            used[i] = true;
        }
        CHECK(index_sorted(r));
    }
    int count = 0;
    for (bool u : used) count += u ? 1 : 0;
    CHECK(r.tunable_count == count);
}

NNH_TEST(live_registry_finds_by_hash_and_generations_go_stale) {
    if constexpr (!nnh::live::kEnabled) return;
    restart();
    float a = 1;
    auto h = nnh::live::tune("p/a", &a, {.min = 0, .max = 2, .step = 1});
    const std::uint32_t id = h.id();
    CHECK(tunable_of(*g_registry, id) != nullptr);
    CHECK(find_tunable(*g_registry, nnh::detail::fnv1a32("p/a")) == id_slot(id));
    h = nnh::live::Handle{};
    CHECK(tunable_of(*g_registry, id) == nullptr);
    CHECK(find_tunable(*g_registry, nnh::detail::fnv1a32("p/a")) < 0);
    auto again = nnh::live::tune("p/a", &a, {.min = 0, .max = 2, .step = 1});
    CHECK(id_slot(again.id()) == id_slot(id)); // the lowest free slot
    CHECK(again.id() != id);                   // a new generation
    CHECK(tunable_of(*g_registry, id) == nullptr);
}

NNH_TEST(live_registry_capacity_plus_one_asserts) {
    if constexpr (!nnh::live::kEnabled) return;
    restart();
    nnh::live::configure({.max_tunables = 4, .max_watches = 2, .max_shown = 2});
    static nnh::live::Handle hs[4];
    for (int i = 0; i < 4; ++i) {
        char name[16];
        std::snprintf(name, sizeof name, "t%d", i);
        hs[i] = nnh::live::tune(nnh::live::Name::runtime(name), &g_values[i], {.min = -1, .max = 1, .step = 1});
        CHECK(hs[i].valid());
    }
    CHECK(g_registry->max_tunables == 4);
    CHECK(asserts_or_is_ignored([] { auto h = nnh::live::tune("t4", &g_values[4], {.min = -1, .max = 1, .step = 1}); }));
    for (auto& h : hs) h = nnh::live::Handle{};
}

NNH_TEST(live_registry_configure_after_use_asserts) {
    if constexpr (!nnh::live::kEnabled) return;
    restart();
    static float v = 0;
    static nnh::live::Handle h;
    h = nnh::live::tune("x", &v, {.min = 0, .max = 1, .step = 1});
    CHECK(asserts_or_is_ignored([] { nnh::live::configure({}); }));
    h = nnh::live::Handle{};
}

NNH_TEST(live_registry_refuses_names_used_twice_or_as_groups) {
    if constexpr (!nnh::live::kEnabled) return;
    restart();
    static float v = 0;
    static nnh::live::Handle h;
    h = nnh::live::tune("player/jump", &v, {.min = 0, .max = 1, .step = 1});
    CHECK(asserts_or_is_ignored([] { auto x = nnh::live::tune("player/jump", &v, {.min = 0, .max = 1, .step = 1}); }));
    CHECK(asserts_or_is_ignored([] { auto x = nnh::live::tune("player", &v, {.min = 0, .max = 1, .step = 1}); }));
    CHECK(asserts_or_is_ignored(
        [] { auto x = nnh::live::tune("player/jump/high", &v, {.min = 0, .max = 1, .step = 1}); }));
    CHECK(asserts_or_is_ignored([] { auto x = nnh::live::watch("player/jump", &v); }));
    h = nnh::live::Handle{};
}

NNH_TEST(live_registry_refuses_run_time_names_breaking_a_rule) {
    if constexpr (!nnh::live::kEnabled) return;
    restart();
    static float v = 0;
    CHECK(asserts_or_is_ignored([] {
        auto x = nnh::live::tune(nnh::live::Name::runtime("bad name"), &v, {.min = 0, .max = 1, .step = 1});
    }));
    CHECK(asserts_or_is_ignored([] {
        auto x = nnh::live::tune(nnh::live::Name::runtime("abcdefghijklmnopqrstuvwxyz0123456789"), &v,
                                 {.min = 0, .max = 1, .step = 1});
    }));
}
