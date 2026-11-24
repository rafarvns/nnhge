// Links, attach and detach (spec 022 FR-6, FR-20, T3.2, T3.3): link invariants after seeded sequences (a brute-force
// check), append order, detach, attaching to the current parent, each refusal in the plan's order — false, an assert,
// a stats refusal, the state hash unchanged — and can_attach answering the same without asserting; no events.

#include "hierarchy_fixture.hpp"
#include "nnh/events.hpp"

using nnh::AttachCheck;
using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Reparent;
using nnh::Vec2;
using nnh::World;
using namespace hierarchy_test;

namespace {

struct Rng {
    std::uint32_t s;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
};

Hierarchy* g_h = nullptr;
Entity g_child{}, g_parent{};
bool g_result = true;
void attach_g() { g_result = g_h->attach(g_child, g_parent); }

/// The refusal for `why`: attach asserts (Release: false), counts it, and changes nothing else.
bool refused(World& w, Hierarchy& h, Entity child, Entity parent, AttachCheck why) {
    if (h.can_attach(child, parent) != why) return false;
    const std::uint32_t before = state_hash(w);
    const int refusals = h.stats().refusals;
    g_h = &h;
    g_child = child;
    g_parent = parent;
    g_result = true;
    const bool asserted = asserts_or_is_ignored(attach_g);
    world_test::recover(w);
    nnh::detail::world_module_state(w).words[nnh::detail::kRefusals] = static_cast<std::uint32_t>(refusals);
    const bool same = state_hash(w) == before;
    return asserted && same && (nnh::build::kAssertsEnabled || !g_result);
}

} // namespace

NNH_TEST(attach_appends_children_in_order) {
    World w = make_world();
    Hierarchy h(w);
    const Entity p = spawn_node(w);
    Entity c[4];
    for (Entity& e : c) {
        e = spawn_node(w);
        CHECK(h.attach(e, p));
    }
    int i = 0;
    for (Entity e : h.children(p)) CHECK(e == c[i++]);
    CHECK(i == 4 && h.child_count(p) == 4 && h.first_child(p) == c[0] && h.next_sibling(c[3]) == Entity{});
    CHECK(h.detach(c[1]) && h.child_count(p) == 3 && h.next_sibling(c[0]) == c[2] && h.parent(c[1]) == Entity{});
    CHECK(h.attach(c[1], p) && h.next_sibling(c[3]) == c[1]); // re-attached at the end
    check_links(w);
}

NNH_TEST(attach_link_invariants_hold_over_seeded_sequences) {
    for (std::uint32_t seed = 1; seed <= 10; ++seed) {
        World w = make_world(48, 48);
        Hierarchy h(w);
        Rng rng{seed * 0x9E3779B9u};
        Entity e[40];
        for (Entity& x : e) x = spawn_node(w, {.position = {static_cast<float>(rng.next() % 64), 0}});
        for (int step = 0; step < 3000; ++step) {
            Entity& a = e[rng.next() % 40];
            const Entity b = e[rng.next() % 40];
            const std::uint32_t op = rng.next() % 10;
            if (op < 5) {
                const Reparent mode = (op & 1) != 0 ? Reparent::KeepLocal : Reparent::KeepWorld;
                if (h.can_attach(a, b, mode) == AttachCheck::Ok) CHECK(h.attach(a, b, mode));
            } else if (op < 7) {
                if (w.alive(a)) CHECK(h.detach(a, op == 5 ? Reparent::KeepWorld : Reparent::KeepLocal));
            } else if (op == 7 && w.alive(a)) {
                h.detach_children(a);
            } else if (op == 8 && w.alive(a)) {
                w.destroy(a);
            } else if (!w.alive(a)) {
                w.flush();
                a = spawn_node(w);
            }
            if ((step & 31) == 0) check_links(w);
        }
        check_links(w);
    }
}

NNH_TEST(attach_to_the_current_parent_changes_nothing) {
    World w = make_world();
    Hierarchy h(w);
    const Entity p = spawn_node(w), a = spawn_node(w), b = spawn_node(w);
    h.attach(a, p);
    h.attach(b, p);
    const std::uint32_t before = state_hash(w);
    CHECK(h.attach(a, p) && h.attach(a, p, Reparent::KeepLocal));
    CHECK(state_hash(w) == before && h.first_child(p) == a); // a stays first
}

NNH_TEST(attach_refusals_in_order_change_nothing) {
    World w = make_world();
    Hierarchy h(w);
    const Entity root = spawn_node(w);
    const Entity child = spawn_node(w);
    h.attach(child, root);
    const Entity plain = w.spawn(); // no node
    const Entity dead = spawn_node(w);
    w.destroy(dead);
    CHECK(refused(w, h, Entity{}, root, AttachCheck::Stale));
    CHECK(refused(w, h, dead, root, AttachCheck::Stale));
    CHECK(refused(w, h, plain, root, AttachCheck::NoNode));
    CHECK(refused(w, h, root, plain, AttachCheck::NoNode));
    CHECK(refused(w, h, root, root, AttachCheck::Cycle));
    CHECK(refused(w, h, root, child, AttachCheck::Cycle));
    // Stale wins over NoNode, whichever side.
    CHECK(h.can_attach(plain, dead) == AttachCheck::Stale);
    // A chain of 16 levels (depths 0..15): one more level is refused.
    Entity chain[16];
    chain[0] = spawn_node(w);
    for (int i = 1; i < 16; ++i) {
        chain[i] = spawn_node(w);
        CHECK(h.attach(chain[i], chain[i - 1]));
    }
    CHECK(h.depth(chain[15]) == 15);
    const Entity extra = spawn_node(w);
    CHECK(refused(w, h, extra, chain[15], AttachCheck::TooDeep));
    CHECK(refused(w, h, chain[0], extra, AttachCheck::TooDeep)); // a subtree of height 15 under a root
    // Keep-world under a zero scale: refused; keep-local accepted.
    const Entity flat = spawn_node(w, {.scale = {0, 1}});
    CHECK(refused(w, h, extra, flat, AttachCheck::NotInvertible));
    CHECK(h.can_attach(extra, flat, Reparent::KeepLocal) == AttachCheck::Ok);
    CHECK(h.stats().refusals == 0); // `refused` restores the count after each check
    g_h = &h;
    g_child = extra;
    g_parent = flat;
    asserts_or_is_ignored(attach_g);
    world_test::recover(w);
    CHECK(h.stats().refusals == 1);
    g_h = nullptr;
}

NNH_TEST(attach_dying_entities_count_as_stale) {
    static AttachCheck seen = AttachCheck::Ok;
    static Entity other{};
    const nnh::ComponentDesc kinds[] = {nnh::node_component(8, [](World& w, Entity e, Node&) {
        seen = Hierarchy(w).can_attach(other, e);
    })};
    auto r = World::create({.max_entities = 8}, kinds);
    World w = static_cast<World&&>(r.value());
    const Entity a = spawn_node(w);
    other = spawn_node(w);
    w.destroy(a);
    CHECK(seen == AttachCheck::Stale);
}

NNH_TEST(attach_publishes_no_event) {
    World w = make_world();
    Hierarchy h(w);
    const auto before = nnh::events::stats();
    const Entity p = spawn_node(w), c = spawn_node(w);
    h.attach(c, p);
    h.detach(c, Reparent::KeepLocal);
    h.attach(c, p, Reparent::KeepLocal);
    w.destroy(p);
    const auto after = nnh::events::stats();
    CHECK(after.subscribers == before.subscribers && after.dropped_events == before.dropped_events &&
          after.queue_bytes == before.queue_bytes); // (the bus counts nothing else: the module includes no event header)
}

NNH_TEST(detach_of_a_root_is_true_and_changes_nothing) {
    World w = make_world();
    Hierarchy h(w);
    const Entity a = spawn_node(w, {.position = {4, 4}, .rotation = 1});
    const std::uint32_t before = state_hash(w);
    CHECK(h.detach(a) && h.detach(a, Reparent::KeepLocal) && state_hash(w) == before);
    CHECK(h.detach_children(a) == 0);
}
