// Walking a subtree (spec 022 FR-7, T4.3): pre-order in child order; destroying the current node, an ancestor, the next
// sibling or an unvisited subtree; detaching and attaching into and out of the unvisited part; spawning (not visited);
// nested walks.

#include "hierarchy_fixture.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Reparent;
using nnh::World;
using namespace hierarchy_test;

namespace {

/// root → a (→ a1, a2 → a21), b (→ b1), c
struct Tree7 {
    World w = make_world();
    Hierarchy h{w};
    Entity root, a, a1, a2, a21, b, b1, c;
    Tree7() {
        root = spawn_node(w);
        a = spawn_node(w);
        a1 = spawn_node(w);
        a2 = spawn_node(w);
        a21 = spawn_node(w);
        b = spawn_node(w);
        b1 = spawn_node(w);
        c = spawn_node(w);
        h.attach(a, root);
        h.attach(a1, a);
        h.attach(a2, a);
        h.attach(a21, a2);
        h.attach(b, root);
        h.attach(b1, b);
        h.attach(c, root);
    }
};

template <int N>
bool visited_is(const Entity (&got)[32], int n, const Entity (&want)[N]) {
    if (n != N) return false;
    for (int i = 0; i < N; ++i)
        if (got[i] != want[i]) return false;
    return true;
}

} // namespace

NNH_TEST(each_descendant_is_pre_order_in_child_order) {
    Tree7 t;
    Entity got[32];
    int n = 0;
    t.h.each_descendant(t.root, [&](Entity e) { got[n++] = e; });
    const Entity want[] = {t.a, t.a1, t.a2, t.a21, t.b, t.b1, t.c};
    CHECK(visited_is(got, n, want));
    n = 0;
    t.h.each_descendant(t.a1, [&](Entity e) { got[n++] = e; });
    CHECK(n == 0);
}

NNH_TEST(each_descendant_destroying_the_current_node_skips_its_subtree) {
    Tree7 t;
    Entity got[32];
    int n = 0;
    t.h.each_descendant(t.root, [&](Entity e) {
        got[n++] = e;
        if (e == t.a2) t.w.destroy(e);
    });
    const Entity want[] = {t.a, t.a1, t.a2, t.b, t.b1, t.c};
    CHECK(visited_is(got, n, want) && !t.w.alive(t.a21));
    check_links(t.w);
}

NNH_TEST(each_descendant_destroying_an_ancestor_or_next_sibling) {
    Tree7 t;
    Entity got[32];
    int n = 0;
    t.h.each_descendant(t.root, [&](Entity e) {
        got[n++] = e;
        if (e == t.a1) t.w.destroy(t.a); // an ancestor: a2, a21 go too
        if (e == t.b) t.w.destroy(t.c);  // the remembered next sibling
    });
    const Entity want[] = {t.a, t.a1, t.b, t.b1};
    CHECK(visited_is(got, n, want));
    check_links(t.w);
}

NNH_TEST(each_descendant_destroying_an_unvisited_subtree) {
    Tree7 t;
    Entity got[32];
    int n = 0;
    t.h.each_descendant(t.root, [&](Entity e) {
        got[n++] = e;
        if (e == t.a) t.w.destroy(t.b);
    });
    const Entity want[] = {t.a, t.a1, t.a2, t.a21, t.c};
    CHECK(visited_is(got, n, want));
}

NNH_TEST(each_descendant_moving_nodes_in_and_out_of_the_unvisited_part) {
    Tree7 t;
    const Entity outside = spawn_node(t.w);
    Entity got[32];
    int n = 0;
    t.h.each_descendant(t.root, [&](Entity e) {
        got[n++] = e;
        if (e == t.a1) {
            t.h.detach(t.b);                     // out of the unvisited part: not visited
            t.h.attach(outside, t.c);            // into it: visited under c
            t.h.attach(t.a21, t.root, Reparent::KeepLocal); // moved from unvisited a2 to the end: visited there
        }
    });
    const Entity expect[] = {t.a, t.a1, t.a2, t.c, outside, t.a21};
    CHECK(visited_is(got, n, expect));
    check_links(t.w);
}

NNH_TEST(each_descendant_skips_nodes_created_during_the_walk) {
    Tree7 t;
    Entity got[32];
    int n = 0;
    t.h.each_descendant(t.root, [&](Entity e) {
        got[n++] = e;
        if (e == t.a) t.h.attach(spawn_node(t.w), t.root);
        if (e == t.b1) t.h.attach(spawn_node(t.w), t.b);
    });
    const Entity want[] = {t.a, t.a1, t.a2, t.a21, t.b, t.b1, t.c};
    CHECK(visited_is(got, n, want) && t.h.child_count(t.root) == 4);
}

NNH_TEST(each_descendant_nested_walks) {
    Tree7 t;
    int outer = 0, inner = 0;
    t.h.each_descendant(t.root, [&](Entity e) {
        ++outer;
        t.h.each_descendant(e, [&](Entity x) {
            ++inner;
            if (x == t.b1) t.w.destroy(x);
        });
    });
    // root's 7 descendants; inner: a → 3, a2 → 1, b → 1 (b1 destroyed by the inner walk, so b1 isn't an outer visit).
    CHECK(outer == 6 && inner == 5 && nnh::detail::world_module_state(t.w).walks == nullptr);
}
