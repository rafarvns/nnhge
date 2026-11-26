// Misuse and random operations (spec 030 T2.7, FR-6, FR-18): 100 000 seeded registrations, removals, edits, shows and
// resets over variables with guard bytes around them; invariants: values in range, guards intact, counts consistent,
// the indexes sorted.

#include <cstdio>
#include <cstring>

#include "live/live_apply.hpp"
#include "live/live_registry.hpp"
#include "live_fixture.hpp"

using namespace nnh::detail::live;
using live_test::restart;
namespace live = nnh::live;

namespace {

constexpr int kVars = 48;
constexpr std::uint32_t kGuard = 0xA5A5A5A5u;

struct Guarded {
    std::uint32_t before = kGuard;
    float value = 0;
    std::uint32_t after = kGuard;
};

Guarded g_vars[kVars];
live::Handle g_handles[kVars];

std::uint32_t next(std::uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
}

bool sorted(const Registry& r) {
    for (int i = 1; i < r.tunable_count; ++i) {
        const Entry& a = r.entries[r.tunable_index[i - 1]];
        const Entry& b = r.entries[r.tunable_index[i]];
        if (std::strcmp(a.name, b.name) >= 0) return false;
    }
    return true;
}

} // namespace

NNH_TEST(live_random_ops_keep_every_invariant) {
    if constexpr (!live::kEnabled) return;
    restart();
    (void)registry();
    std::uint32_t s = 30;
    for (auto& v : g_vars) v = Guarded{};
    for (int op = 0; op < 100000; ++op) {
        const int i = static_cast<int>(next(s) % kVars);
        switch (next(s) % 6) {
        case 0:
            if (!g_handles[i].valid()) {
                char name[24];
                std::snprintf(name, sizeof name, "r%d/v%d", i % 4, i);
                g_vars[i].value = static_cast<float>(i % 5);
                g_handles[i] = live::tune(live::Name::runtime(name), &g_vars[i].value, {.min = 0, .max = 8, .step = 0.5f});
            }
            break;
        case 1: g_handles[i] = live::Handle{}; break;
        case 2:
            if (g_handles[i].valid()) {
                set_pending(*g_registry, id_slot(g_handles[i].id()), float_bits(static_cast<float>(next(s) % 17) * 0.5f));
            }
            break;
        case 3: {
            RemoteOp o;
            o.op = kOpSet;
            char name[24];
            std::snprintf(name, sizeof name, "r%d/v%d", i % 4, i);
            o.hash = nnh::detail::fnv1a32(name);
            o.bits = next(s) % 4 == 0 ? 0x7F800000u : float_bits(static_cast<float>(next(s) % 20) * 0.5f);
            queue_op(*g_registry, o);
            break;
        }
        case 4: live::show("r/shown", op); break;
        default:
            if (next(s) % 8 == 0) CHECK(nnh::frame());
            if (next(s) % 64 == 0) live::reset_all();
            break;
        }
        if (op % 1000 == 0) CHECK(sorted(*g_registry));
    }
    CHECK(nnh::frame());
    int live_count = 0;
    for (int i = 0; i < kVars; ++i) {
        CHECK(g_vars[i].before == kGuard && g_vars[i].after == kGuard);
        CHECK(g_vars[i].value >= 0 && g_vars[i].value <= 8);
        live_count += g_handles[i].valid() ? 1 : 0;
    }
    CHECK(g_registry->tunable_count == live_count);
    CHECK(g_registry->pending_count == 0);
    for (auto& h : g_handles) h = live::Handle{};
}

NNH_TEST(live_misuse_is_harmless_on_the_release_path) {
    // Every misuse of the plan asserts in Debug/Development (test_live_registry, test_live_tune, test_live_watch); in
    // Release the same calls compile to nothing.
    if constexpr (live::kEnabled) return;
    static float v = 5;
    auto a = live::tune("x", &v, {.min = 1, .max = 1, .step = 0});
    auto b = live::tune("x", &v, {.min = 0, .max = 1, .step = 1});
    live::Handle stale;
    CHECK(!stale.changed());
    stale.reset();
    live::configure({});
    CHECK(v == 5);
}
