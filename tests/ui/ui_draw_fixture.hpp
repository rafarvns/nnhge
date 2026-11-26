#pragma once
// Drawing helpers for the UI tests (spec 028 stage 6): the items one frame of draws records on the fake platform, and
// a field-by-field comparison, so a UI's draw is compared with the same gfx calls written by hand.

#include <bit>

#include "nnh/platform_fake.hpp"
#include "ui_fixture.hpp"

namespace ui_test {

using Item = nnh::platform::DrawItem2D;

/// The items `draw` records in one frame (the frame's other draws: none). @return how many (at most `capacity` kept).
template <typename Fn>
int capture(Fn&& draw, Item* out, int capacity) {
    CHECK(nnh::frame());
    draw();
    CHECK(nnh::frame());
    const auto& s = nnh::platform_fake::state();
    const int n = s.last_frame_item_count;
    for (int i = 0; i < n && i < capacity; ++i) out[i] = s.last_frame_items[i];
    return n;
}

inline bool same_item(const Item& a, const Item& b) {
    auto f = [](float v) { return std::bit_cast<std::uint32_t>(v); };
    return a.kind == b.kind && a.layer == b.layer && a.texture == b.texture && a.region.x == b.region.x &&
           a.region.y == b.region.y && a.region.w == b.region.w && a.region.h == b.region.h && f(a.x) == f(b.x) &&
           f(a.y) == f(b.y) && f(a.w) == f(b.w) && f(a.h) == f(b.h) && a.color.r == b.color.r &&
           a.color.g == b.color.g && a.color.b == b.color.b && a.color.a == b.color.a;
}

inline bool same_items(const Item* a, int na, const Item* b, int nb) {
    if (na != nb) return false;
    for (int i = 0; i < na; ++i)
        if (!same_item(a[i], b[i])) return false;
    return true;
}

inline nnh::Rect frect(const nnh::UiRect& r) { return r.rect(); }

} // namespace ui_test
