// The capped, sorted hit list behind the "all hits" queries (spec 019 FR-11).

#include "collision/hit_list.hpp"
#include "test.hpp"

#include <initializer_list>

namespace collision = nnh::collision;
using collision::RayHit;
using collision::detail::append_index;
using collision::detail::insert_hit;

namespace {

RayHit hit(float fraction, int box) {
    RayHit h;
    h.fraction = fraction;
    h.box = box;
    return h;
}

} // namespace

NNH_TEST(hits_are_kept_sorted_by_fraction) {
    RayHit hits[4];
    int total = 0;
    total = insert_hit(hits, 4, total, hit(0.5f, 0));
    total = insert_hit(hits, 4, total, hit(0.25f, 1));
    total = insert_hit(hits, 4, total, hit(0.75f, 2));
    total = insert_hit(hits, 4, total, hit(0.0f, 3));
    CHECK(total == 4);
    CHECK(hits[0].box == 3 && hits[1].box == 1 && hits[2].box == 0 && hits[3].box == 2);
}

NNH_TEST(equal_fractions_keep_their_insertion_order) {
    RayHit hits[3];
    int total = 0;
    total = insert_hit(hits, 3, total, hit(0.5f, 4));
    total = insert_hit(hits, 3, total, hit(0.5f, 7));
    total = insert_hit(hits, 3, total, hit(0.25f, 9));
    CHECK(hits[0].box == 9 && hits[1].box == 4 && hits[2].box == 7);
}

NNH_TEST(an_overflowing_list_keeps_the_nearest_and_counts_all) {
    RayHit hits[2];
    int total = 0;
    for (const float f : {0.9f, 0.1f, 0.8f, 0.2f, 0.5f})
        total = insert_hit(hits, 2, total, hit(f, static_cast<int>(f * 10)));
    CHECK(total == 5);
    CHECK(hits[0].fraction == 0.1f && hits[1].fraction == 0.2f);
}

NNH_TEST(a_tie_beyond_the_capacity_is_dropped) {
    RayHit hits[1];
    int total = insert_hit(hits, 1, 0, hit(0.5f, 1));
    total = insert_hit(hits, 1, total, hit(0.5f, 2));
    CHECK(total == 2 && hits[0].box == 1);
}

NNH_TEST(a_null_list_with_capacity_zero_only_counts) {
    int total = 0;
    for (int i = 0; i < 3; ++i)
        total = insert_hit(nullptr, 0, total, hit(0.1f, i));
    CHECK(total == 3);
    int n = 0;
    for (int i = 0; i < 5; ++i)
        n = append_index(nullptr, 0, n, i);
    CHECK(n == 5);
}

NNH_TEST(indices_are_appended_up_to_the_capacity) {
    int out[2] = {-1, -1};
    int n = 0;
    for (int i = 3; i < 6; ++i)
        n = append_index(out, 2, n, i);
    CHECK(n == 3 && out[0] == 3 && out[1] == 4);
}
