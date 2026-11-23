// The self-check's 16 prefabs written out flattened by hand (spec 021 AC-2): what the hand-written side spawns. The
// prefab side registers the same things through the library, with variants (self_check_sides.cpp); nothing here is
// read from the library, so a flattening mistake shows as a difference between the two worlds.

#include "check_kinds.hpp"

#include <initializer_list>

namespace check {

B b_seq(int n) {
    B b;
    for (int i = 0; i < 16; ++i) b.v[i] = n * 100 + i;
    return b;
}

namespace {

struct Table {
    HandDef defs[kPrefabs];

    static void kinds(HandDef& d, std::initializer_list<int> list) {
        for (int k : list) d.kinds[d.kind_count++] = k;
    }
    static void tags(HandDef& d, std::initializer_list<int> list) {
        for (int t : list) d.tags[d.tag_count++] = t;
    }
    static void hooks(HandDef& d, std::initializer_list<int> list) {
        for (int h : list) d.hooks[d.hook_count++] = h;
    }

    Table() {
        HandDef* d = defs;
        // 0 grunt: A, B1; enemy; A's box.
        kinds(d[0], {kA, kB}), d[0].v.a = {{0, 0, 8, 8}, 1}, d[0].v.b = b_seq(1), tags(d[0], {kEnemy});
        d[0].place = Place::ABox;
        // 1 coin: A, E; coin; A's box.
        kinds(d[1], {kA, kE}), d[1].v.a = {{0, 0, 6, 6}, 2}, d[1].v.e = {1, 2}, tags(d[1], {kCoin});
        d[1].place = Place::ABox;
        // 2 mover: C, D7; fx; C; hook info.
        kinds(d[2], {kC, kD}), d[2].v.c = {{1, 2}}, d[2].v.d = D(7), tags(d[2], {kFx}), d[2].place = Place::CVec;
        hooks(d[2], {kHookInfo});
        // 3 boss: A, B2, D9; enemy, boss; A's box; hook mark.
        kinds(d[3], {kA, kB, kD}), d[3].v.a = {{0, 0, 16, 16}, 3}, d[3].v.b = b_seq(2), d[3].v.d = D(9);
        tags(d[3], {kEnemy, kBoss}), d[3].place = Place::ABox, hooks(d[3], {kHookMark});
        // 4 pickup: E; pickup.
        kinds(d[4], {kE}), d[4].v.e = {3, 4}, tags(d[4], {kPickup});
        // 5 burst: no component; hook child.
        hooks(d[5], {kHookChild});
        // 6 flying_grunt = grunt + C(5, 5) appended + flying.
        kinds(d[6], {kA, kB, kC}), d[6].v.a = {{0, 0, 8, 8}, 1}, d[6].v.b = b_seq(1), d[6].v.c = {{5, 5}};
        tags(d[6], {kEnemy, kFlying}), d[6].place = Place::ABox;
        // 7 rich_coin = coin with E(9, 9) in place.
        kinds(d[7], {kA, kE}), d[7].v.a = {{0, 0, 6, 6}, 2}, d[7].v.e = {9, 9}, tags(d[7], {kCoin});
        d[7].place = Place::ABox;
        // 8 ace = flying_grunt with B3 in place, D3 appended, boss, hook mark (a variant of a variant).
        kinds(d[8], {kA, kB, kC, kD}), d[8].v.a = {{0, 0, 8, 8}, 1}, d[8].v.b = b_seq(3), d[8].v.c = {{5, 5}};
        d[8].v.d = D(3), tags(d[8], {kEnemy, kFlying, kBoss}), d[8].place = Place::ABox, hooks(d[8], {kHookMark});
        // 9 suicide: A, D11; fx; A's box; hook suicide.
        kinds(d[9], {kA, kD}), d[9].v.a = {{0, 0, 4, 4}, 5}, d[9].v.d = D(11), tags(d[9], {kFx});
        d[9].place = Place::ABox, hooks(d[9], {kHookSuicide});
        // 10 dropper = mover + E(6, 6) appended + hook child (after info).
        kinds(d[10], {kC, kD, kE}), d[10].v.c = {{1, 2}}, d[10].v.d = D(7), d[10].v.e = {6, 6}, tags(d[10], {kFx});
        d[10].place = Place::CVec, hooks(d[10], {kHookInfo, kHookChild});
        // 11 wide: every kind; four tags; C.
        kinds(d[11], {kA, kB, kC, kD, kE}), d[11].v.a = {{0, 0, 2, 2}, 6}, d[11].v.b = b_seq(4);
        d[11].v.c = {{7, 8}}, d[11].v.d = D(2), d[11].v.e = {5, 6}, tags(d[11], {kEnemy, kCoin, kFx, kPickup});
        d[11].place = Place::CVec;
        // 12 marked_dropper = dropper + boss + hook mark (after info, child).
        kinds(d[12], {kC, kD, kE}), d[12].v.c = {{1, 2}}, d[12].v.d = D(7), d[12].v.e = {6, 6};
        tags(d[12], {kFx, kBoss}), d[12].place = Place::CVec, hooks(d[12], {kHookInfo, kHookChild, kHookMark});
        // 13 plain_e: E(0, 1).
        kinds(d[13], {kE}), d[13].v.e = {0, 1};
        // 14 plain_bd: B5, D1.
        kinds(d[14], {kB, kD}), d[14].v.b = b_seq(5), d[14].v.d = D(1);
        // 15 spark: C; fx; C.
        kinds(d[15], {kC}), d[15].v.c = {{0, 0}}, tags(d[15], {kFx}), d[15].place = Place::CVec;
    }
};

} // namespace

const HandDef& hand_def(int index) {
    static const Table table;
    return table.defs[index];
}

} // namespace check
