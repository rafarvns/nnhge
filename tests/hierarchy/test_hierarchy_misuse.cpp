// Misuse and cost (spec 022 FR-20, FR-21, FR-23, T4.8): every FR-23 case asserts in Debug/Development and is harmless
// in Release (the pc-release run takes the other branch); nothing allocates after the world's creation over 100 000
// operations; nothing is logged in the normal path.

#include "alloc_counter.hpp"
#include "hierarchy_fixture.hpp"
#include "nnh/app.hpp"
#include "nnh/memory.hpp"

#include <cstring>
#include <limits>

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Reparent;
using nnh::World;
using namespace hierarchy_test;

namespace {

Hierarchy* g_h = nullptr;
World* g_w = nullptr;
Entity g_a{}, g_b{};
bool g_ok = true;
int g_n = -1;

void attach_ab() { g_ok = g_h->attach(g_a, g_b); }
void detach_a() { g_ok = g_h->detach(g_a); }
void detach_children_a() { g_n = g_h->detach_children(g_a); }
void pass() { g_n = g_h->propagate(); }
void view_of_g_w() { Hierarchy h(*g_w); }
void attach_on_empty_view() {
    Hierarchy empty;
    g_ok = empty.attach(g_a, g_b);
}

struct Rng {
    std::uint32_t s;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
};

} // namespace

NNH_TEST(hierarchy_misuse_asserts_or_is_harmless) {
    World w = make_world();
    Hierarchy h(w);
    g_h = &h;
    g_w = &w;
    const Entity a = spawn_node(w), b = spawn_node(w), plain = w.spawn(), gone = spawn_node(w);
    w.destroy(gone);
    h.attach(b, a);
    struct Case {
        Entity x, y;
    };
    const Case attaches[] = {{Entity{}, a}, {gone, a}, {plain, a}, {a, plain}, {a, a}, {a, b}};
    for (const Case& c : attaches) {
        g_a = c.x;
        g_b = c.y;
        g_ok = true;
        CHECK(asserts_or_is_ignored(attach_ab));
        world_test::recover(w);
        if (!nnh::build::kAssertsEnabled) CHECK(!g_ok);
    }
    for (Entity e : {Entity{}, gone, plain}) {
        g_a = e;
        CHECK(asserts_or_is_ignored(detach_a));
        CHECK(asserts_or_is_ignored(detach_children_a));
    }
    CHECK(asserts_or_is_ignored(attach_on_empty_view));
    // A non-finite local reaching the pass: Release treats it as the identity and counts it.
    h.local(b)->position.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(asserts_or_is_ignored(pass));
    if (!nnh::build::kAssertsEnabled) {
        CHECK(g_n == 2 && h.stats().bad_locals == 1);
        CHECK(h.world_transform(b)->position() == h.world_transform(a)->position()); // the identity under a
        CHECK(same_bits(h.world_now(b), *h.world_transform(b)));
    }
    h.local(b)->position.x = 0;
    // A world without the node kind.
    World plain_world = world_test::make_world();
    g_w = &plain_world;
    CHECK(asserts_or_is_ignored(view_of_g_w));
    CHECK(h.stats().nodes == 2);
    check_links(w);
    g_h = nullptr;
    g_w = nullptr;
}

NNH_TEST(hierarchy_allocates_and_logs_nothing_in_the_normal_path) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({}));
    World w = make_world(256, 256);
    Hierarchy h(w);
    Entity e[200];
    for (Entity& x : e) x = spawn_node(w);
    const int live_before = nnh::mem::live_allocations();
    const auto news_before = alloc_counter::news();
    char last_line[256];
    std::strncpy(last_line, nnh::platform_fake::state().last_console_line, sizeof last_line - 1);
    last_line[sizeof last_line - 1] = '\0';
    Rng rng{12345};
    for (int step = 0; step < 100'000; ++step) {
        Entity& a = e[rng.next() % 200];
        const Entity b = e[rng.next() % 200];
        switch (rng.next() % 8) {
        case 0:
        case 1:
            if (h.can_attach(a, b) == nnh::AttachCheck::Ok) h.attach(a, b, (step & 1) ? Reparent::KeepLocal : Reparent::KeepWorld);
            break;
        case 2:
            if (w.alive(a)) h.detach(a);
            break;
        case 3:
            if (w.alive(a)) h.local(a)->rotation += 0.01f;
            break;
        case 4:
            if (w.alive(a)) (void)h.world_now(a);
            break;
        case 5:
            if ((step & 63) == 0) h.propagate();
            break;
        case 6:
            if ((step & 15) == 0 && w.alive(a)) w.destroy(a);
            break;
        default:
            if (!w.alive(a)) {
                w.flush();
                a = spawn_node(w);
            }
            break;
        }
    }
    CHECK(alloc_counter::news() == news_before && nnh::mem::live_allocations() == live_before);
    CHECK(std::strncmp(nnh::platform_fake::state().last_console_line, last_line, sizeof last_line - 1) == 0);
    nnh::shutdown();
}
