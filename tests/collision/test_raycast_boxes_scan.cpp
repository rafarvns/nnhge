// Rays against boxes equal the slab test box by box (plan "Console bench refinement (round 2)"): the scan's
// rejects, its inline entry test and its best-entry selection must answer exactly as slab_cast() on every box
// in index order, the lowest index winning a tie at one fraction, on lattice and off-lattice rays.

#include "collision/exact.hpp"
#include "collision/ray_box.hpp"
#include "collision/ray_hit.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace collision = nnh::collision;
namespace detail = collision::detail;
using collision::RayHit;
using nnh::Rect;
using nnh::Vec2;

namespace {

struct Hash {
    std::uint32_t s;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    int range(int n) { return static_cast<int>(next() % static_cast<std::uint32_t>(n)); }
};

/// The hit of one box as slab_cast() and the spec's hit rules give it; false for a miss or a skipped start
/// inside.
bool brute_hit(const detail::Segment& s, Vec2 from, Rect box, bool from_inside, RayHit& hit) {
    if (!(box.w > 0 && box.h > 0)) return false;
    const detail::SlabHit h = detail::slab_cast(s, detail::slab_x(box), detail::slab_y(box));
    if (!h.hit || (h.inside && !from_inside)) return false;
    if (h.inside) {
        hit = detail::inside_hit(from);
        return true;
    }
    const float x1 = box.x + box.w, y1 = box.y + box.h;
    hit = {};
    hit.fraction = detail::to_fraction(h.t);
    hit.normal = detail::face_normal(s, h.face_x);
    if (h.face_x) {
        hit.point = {s.sx > 0 ? box.x : x1, detail::clamp_to(detail::along(s.py, s.dy, h.t), box.y, y1)};
    } else {
        hit.point = {detail::clamp_to(detail::along(s.px, s.dx, h.t), box.x, x1), s.sy > 0 ? box.y : y1};
    }
    return true;
}

bool same(const RayHit& a, const RayHit& b) {
    return std::memcmp(&a.fraction, &b.fraction, 4) == 0 && std::memcmp(&a.distance, &b.distance, 4) == 0 &&
           std::memcmp(&a.point, &b.point, 8) == 0 && std::memcmp(&a.normal, &b.normal, 8) == 0 &&
           a.box == b.box && a.inside == b.inside;
}

/// A coordinate: on the lattice (often on another box's side) or an arbitrary float.
float coordinate(Hash& h, bool lattice) {
    if (lattice) return static_cast<float>(h.range(256 * 64)) / 256.0f - 8.0f;
    return static_cast<float>(h.range(1 << 20)) / 16411.0f - 8.0f;
}

} // namespace

NNH_TEST(rays_against_boxes_equal_the_slab_test_box_by_box) {
    Hash h{0x1B873593u};
    Rect boxes[24];
    collision::Layers layers[24];
    RayHit all[24];
    int mismatches = 0, hits = 0, inside = 0;
    for (int ray = 0; ray < 6000; ++ray) {
        const bool lattice = ray % 3 != 0;
        for (int i = 0; i < 24; ++i) {
            const float w = h.range(8) == 0 ? 0.0f : static_cast<float>(1 + h.range(24 * 16)) / 16.0f;
            boxes[i] = {coordinate(h, lattice), coordinate(h, lattice), w,
                        static_cast<float>(1 + h.range(384)) / 16.0f};
            if (i > 0 && h.range(6) == 0) boxes[i] = boxes[h.range(i)]; // the same box twice: a tie
            layers[i] = collision::layer(h.range(3));
        }
        Vec2 from{coordinate(h, lattice), coordinate(h, lattice)};
        Vec2 to{coordinate(h, lattice), coordinate(h, lattice)};
        if (h.range(5) == 0) to.x = from.x;
        if (h.range(5) == 0) to.y = from.y;
        if (h.range(4) == 0) from = {boxes[3].x, boxes[3].y + 1}; // flush on a face
        if (lattice && h.range(3) == 0) { // through a box's corner: entering there, or only touching it
            const Rect& b = boxes[h.range(24)];
            const Vec2 corner{h.range(2) ? b.x : b.x + b.w, h.range(2) ? b.y : b.y + b.h};
            const Vec2 d{static_cast<float>(h.range(9) - 4), static_cast<float>(h.range(9) - 4)};
            const float back = static_cast<float>(h.range(5)), ahead = static_cast<float>(1 + h.range(5));
            from = {corner.x - d.x * back, corner.y - d.y * back};
            to = {corner.x + d.x * ahead, corner.y + d.y * ahead};
        }
        const collision::QueryFilter filter{.mask = h.range(4) == 0 ? 0x3u : collision::kAllLayers,
                                            .ignore = h.range(4) == 0 ? 5 : -1,
                                            .hit_from_inside = h.range(2) == 0};
        const collision::BoxView view = collision::box_view(boxes, layers, 24);
        const detail::Segment s = detail::make_segment(from, to);
        // The reference: every box in order, the strictly smaller fraction replacing (ties: the lowest
        // index).
        RayHit want, one;
        bool found = false;
        std::vector<RayHit> want_all;
        for (int i = 0; i < 24 && !detail::zero_length(s); ++i) {
            if (i == filter.ignore || !collision::detects(filter.mask, layers[i])) continue;
            if (!brute_hit(s, from, boxes[i], filter.hit_from_inside, one)) continue;
            one.box = i;
            one.distance = one.fraction * detail::ray_length(from, to);
            want_all.push_back(one);
            if (!found || one.fraction < want.fraction) want = one;
            found = true;
        }
        RayHit got;
        const bool got_found = collision::raycast(view, from, to, got, filter);
        const int n = collision::raycast_all(view, from, to, all, 24, filter);
        bool ok = got_found == found && (!found || same(got, want)) && n == static_cast<int>(want_all.size());
        for (int k = 0; ok && k < n; ++k) {
            bool listed = false;
            for (const RayHit& w : want_all)
                listed = listed || same(all[k], w);
            ok = listed && (k == 0 || !(all[k].fraction < all[k - 1].fraction));
        }
        collision::QueryFilter sight = filter;
        sight.hit_from_inside = false;
        bool blocked = false;
        for (const RayHit& w : want_all)
            blocked = blocked || !w.inside;
        ok = ok && collision::line_of_sight(collision::grid_view(nullptr, 0, 0, 16, 16), view, from, to,
                                            sight) == !blocked;
        if (!ok) ++mismatches;
        hits += found ? 1 : 0;
        inside += found && want.inside ? 1 : 0;
    }
    CHECK(mismatches == 0);
    CHECK(hits > 2500 && inside > 100); // not vacuous
}
