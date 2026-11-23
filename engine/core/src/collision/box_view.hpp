#pragma once
// Reading a BoxView (spec 019 FR-3, FR-4): box i and its layer set, and whether a box is a candidate for a query.
// Internal, pure.
//
// The per-box work is integer only (plan "Console bench refinement"): each field is read as its 32-bit pattern
// (memcpy of one word: a plain load, no aliasing assumption about the game's struct), finiteness and "size > 0" are
// tests on the bits, and finite floats are ordered by order_key() as signed integers. On the 3DS's VFPv2 every
// float compare is a vcmp + vmrs pipeline stall, so the scans keep floats for the few boxes that need a sum.

#include "nnh/collision.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace nnh::collision::detail {

inline std::uint32_t load_bits(const char* p) {
    std::uint32_t b;
    std::memcpy(&b, p, sizeof b);
    return b;
}

inline float from_bits(std::uint32_t b) {
    float v;
    std::memcpy(&v, &b, sizeof v);
    return v;
}

inline std::uint32_t to_bits(float v) {
    std::uint32_t b;
    std::memcpy(&b, &v, sizeof b);
    return b;
}

/// Finite (not NaN, not ±infinity), from the bits.
inline bool finite_bits(std::uint32_t b) {
    return (b & 0x7F800000u) != 0x7F800000u;
}

/// Finite and > 0, from the bits (−0, negatives, NaN and +infinity fail).
inline bool positive_bits(std::uint32_t b) {
    return b - 1u < 0x7F7FFFFFu; // 1 .. 0x7F7FFFFF: the smallest subnormal .. FLT_MAX
}

/// A signed integer with the order of the finite float `b` (−0 and +0 equal): a < b ⇔ key(a) < key(b).
inline std::int32_t order_key(std::uint32_t b) {
    const std::int32_t magnitude = static_cast<std::int32_t>(b & 0x7FFFFFFFu);
    return (b & 0x80000000u) != 0 ? -magnitude : magnitude;
}

/// True when `v` is finite (NaN and infinities fail; no <cmath> needed).
inline bool finite(float v) {
    return finite_bits(to_bits(v));
}

/// A box as its four bit patterns.
struct BoxBits {
    std::uint32_t x = 0, y = 0, w = 0, h = 0;
};

inline Rect to_rect(const BoxBits& b) {
    return {from_bits(b.x), from_bits(b.y), from_bits(b.w), from_bits(b.h)};
}

/// Walks a view in order by byte offsets (no multiply per box): box i's bits and layers, then next().
class BoxCursor {
public:
    explicit BoxCursor(const BoxView& v, int i = 0)
        : boxes_(static_cast<const char*>(v.first)), layers_(static_cast<const char*>(v.layers)),
          box_at_(static_cast<std::ptrdiff_t>(i) * v.stride),
          layers_at_(static_cast<std::ptrdiff_t>(i) * v.layers_stride), stride_(v.stride),
          layers_stride_(v.layers_stride) {}

    BoxBits box() const {
        const char* p = boxes_ + box_at_;
        return {load_bits(p), load_bits(p + 4), load_bits(p + 8), load_bits(p + 12)};
    }
    /// The order keys of x and y alone: the scans' first, cheapest test.
    std::int32_t x_key() const { return order_key(load_bits(boxes_ + box_at_)); }
    std::int32_t y_key() const { return order_key(load_bits(boxes_ + box_at_ + 4)); }
    /// kAllLayers when the view has no layers.
    Layers layers() const { return layers_ == nullptr ? kAllLayers : load_bits(layers_ + layers_at_); }
    void next() {
        box_at_ += stride_;
        layers_at_ += layers_stride_;
    }

private:
    const char* boxes_;
    const char* layers_;
    std::ptrdiff_t box_at_, layers_at_;
    std::ptrdiff_t stride_, layers_stride_;
};

static_assert(sizeof(Rect) == 16 && offsetof(Rect, y) == 4 && offsetof(Rect, w) == 8 && offsetof(Rect, h) == 12,
              "BoxCursor reads a Rect as four 32-bit words");

/// The box under the cursor passes the filter: detected by filter.mask, not filter.ignore (its index is `i`).
inline bool in_filter(const BoxCursor& c, const QueryFilter& filter, int i) {
    return i != filter.ignore && detects(filter.mask, c.layers());
}

/// A box the queries look at: finite, with area.
inline bool valid_box(const BoxBits& box) {
    return positive_bits(box.w) && positive_bits(box.h) && finite_bits(box.x) && finite_bits(box.y);
}

/// The box under the cursor is a candidate: in_filter() and valid_box(). Reads it into `box`. The scans test these
/// after their cheaper position keys (all are ANDed, so the order changes nothing but the time): on the 3DS's
/// single-issue ARM11 an early exit costs less than evaluating every test without branches.
inline bool candidate(const BoxCursor& c, const QueryFilter& filter, int i, BoxBits& box) {
    box = c.box();
    return in_filter(c, filter, i) && valid_box(box);
}

inline Rect box_at(const BoxView& v, int i) {
    return to_rect(BoxCursor(v, i).box());
}

inline Layers layers_at(const BoxView& v, int i) {
    return BoxCursor(v, i).layers();
}

/// candidate() for box i, read into a Rect.
inline bool passes(const BoxView& v, const QueryFilter& filter, int i, Rect& box) {
    BoxBits b;
    if (!candidate(BoxCursor(v, i), filter, i, b)) return false;
    box = to_rect(b);
    return true;
}

} // namespace nnh::collision::detail
