// The integer box scan (plan 019 "Console bench refinement"): order keys and bit tests agree with the float
// operations they replace, the cursor walks a view as box_at() does, and the scans equal contains()/overlaps().

#include "collision/box_view.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

#include <cstdint>

namespace collision = nnh::collision;
namespace detail = nnh::collision::detail;
using nnh::Rect;
using nnh::Vec2;

namespace {

constexpr float kInf = __builtin_inff();
constexpr float kNan = __builtin_nanf("");
constexpr float kMax = 3.40282347e38f;
constexpr float kTiny = 1.4e-45f; // the smallest subnormal

const float kValues[] = {-kMax, -1e20f, -3.5f, -1, -kTiny, -0.0f, 0.0f, kTiny, 1.1754942e-38f, 1e-30f, 0.25f,
                         1,     1.0000001f, 2, 16, 4097.5f, 1e20f, kMax};

/// A fixed sequence (xorshift32): boxes and points on a half-pixel lattice, so edges often coincide.
struct Rng {
    std::uint32_t s = 0x9E3779B9u;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float coord() { return static_cast<float>(static_cast<int>(next() % 81) - 40) * 0.5f; }
    float size() { return static_cast<float>(next() % 9) * 0.5f - 0.5f; } // −0.5 .. 3.5: some without area
};

} // namespace

NNH_TEST(order_keys_order_finite_floats_as_the_floats_compare) {
    for (float a : kValues) {
        for (float b : kValues) {
            const std::int32_t ka = detail::order_key(detail::to_bits(a));
            const std::int32_t kb = detail::order_key(detail::to_bits(b));
            CHECK((a < b) == (ka < kb));
            CHECK((a == b) == (ka == kb)); // −0 and +0 share a key
        }
    }
}

NNH_TEST(bit_tests_match_finite_and_greater_than_zero) {
    const float specials[] = {kInf, -kInf, kNan, -kNan};
    for (float v : kValues) {
        CHECK(detail::finite_bits(detail::to_bits(v)));
        CHECK(detail::positive_bits(detail::to_bits(v)) == (v > 0));
    }
    for (float v : specials) {
        CHECK(!detail::finite_bits(detail::to_bits(v)) && !detail::positive_bits(detail::to_bits(v)));
        CHECK(!detail::finite(v));
    }
}

NNH_TEST(the_cursor_walks_a_struct_view_as_box_at_reads_it) {
    struct Item {
        short hp;
        Rect box;
        collision::Layers layers;
    };
    Item items[4] = {};
    for (int i = 0; i < 4; ++i) {
        items[i].box = {static_cast<float>(i), -static_cast<float>(i), 2.5f, 3};
        items[i].layers = 1u << i;
    }
    const collision::BoxView v = collision::box_view(items, 4, &Item::box, &Item::layers);
    detail::BoxCursor c(v);
    for (int i = 0; i < 4; ++i, c.next()) {
        const Rect r = detail::to_rect(c.box());
        const Rect e = detail::box_at(v, i);
        CHECK(r.x == e.x && r.y == e.y && r.w == e.w && r.h == e.h && c.layers() == items[i].layers);
    }
}

NNH_TEST(point_and_overlap_scans_equal_contains_and_overlaps_on_a_lattice) {
    Rng rng;
    Rect boxes[48];
    for (int round = 0; round < 200; ++round) {
        for (Rect& b : boxes) b = {rng.coord(), rng.coord(), rng.size(), rng.size()};
        boxes[0].x = -0.0f; // a negative zero edge
        const collision::BoxView v = collision::box_view(boxes, 48);
        const Vec2 p{rng.coord(), rng.coord()};
        const Rect q{rng.coord(), rng.coord(), rng.size(), rng.size()};
        int at[48], over[48];
        const int n_at = collision::boxes_at(v, p, at, 48);
        const int n_over = collision::boxes_overlapping(v, q, over, 48);
        int e_at = 0, e_over = 0;
        for (int i = 0; i < 48; ++i) {
            const bool has_area = boxes[i].w > 0 && boxes[i].h > 0;
            if (has_area && collision::contains(boxes[i], p)) CHECK(e_at < n_at && at[e_at++] == i);
            if (collision::overlaps(q, boxes[i])) CHECK(e_over < n_over && over[e_over++] == i);
        }
        CHECK(e_at == n_at && e_over == n_over);
    }
}

NNH_TEST(scans_handle_sums_that_overflow_to_infinity) {
    const Rect boxes[2] = {{3e38f, 0, 3e38f, 1}, {-kMax, -kMax, kMax, kMax}}; // box 0's x + w is +infinity
    const collision::BoxView v = collision::box_view(boxes, 2);
    int out[2];
    CHECK(collision::boxes_at(v, {3.2e38f, 0.5f}, out, 2) == 1 && out[0] == 0);
    CHECK(collision::boxes_overlapping(v, {-kMax, -kMax, 1e38f, 1e38f}, out, 2) == 1 && out[0] == 1);
    CHECK(collision::boxes_overlapping(v, {-kMax, -kMax, 1, 1}, out, 2) == 0); // −kMax + 1 rounds to −kMax
    CHECK(collision::boxes_overlapping(v, {kMax, 0, kMax, 1}, out, 2) == 1 && out[0] == 0);
    collision::RayHit hit;
    CHECK(collision::raycast(v, {3.2e38f, -1}, {3.2e38f, 0.5f}, hit) && hit.box == 0 && hit.normal == Vec2(0, -1));
}
