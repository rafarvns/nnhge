// Simulator window layouts (sim_layout.hpp).

#include "sim_layout.hpp"

#include <cstring>

namespace nnh::platform::pc {
namespace {

constexpr int kTopW = 400;
constexpr int kBottomW = 320;
constexpr int kScreenH = 240;

int small_scale(int scale) { return scale / 2 > 1 ? scale / 2 : 1; }

struct Names {
    SimLayout layout;
    const char* id;
    const char* title;
};

constexpr Names kNames[] = {
    {SimLayout::Stacked, "stacked", "Stacked"},     {SimLayout::SideBySide, "side", "Side by side"},
    {SimLayout::LargeTop, "large", "Large top"},    {SimLayout::TopOnly, "top", "Top only"},
    {SimLayout::BottomOnly, "bottom", "Bottom only"},
};

const Names& names_of(SimLayout layout) {
    for (const Names& n : kNames) {
        if (n.layout == layout) return n;
    }
    return kNames[0];
}

} // namespace

SimSize natural_size(SimLayout layout, int s) {
    switch (layout) {
    case SimLayout::Stacked: return {kTopW * s, 2 * kScreenH * s};
    case SimLayout::SideBySide: return {(kTopW + kBottomW) * s, kScreenH * s};
    case SimLayout::LargeTop: return {kTopW * s + kBottomW * small_scale(s), kScreenH * s};
    case SimLayout::TopOnly: return {kTopW * s, kScreenH * s};
    case SimLayout::BottomOnly: return {kBottomW * s, kScreenH * s};
    }
    return {};
}

int fit_scale(SimLayout layout, int window_w, int window_h) {
    int best = 1;
    for (int s = 1; s <= kMaxFitScale; ++s) {
        const SimSize size = natural_size(layout, s);
        if (size.w <= window_w && size.h <= window_h) best = s;
    }
    return best;
}

LayoutResult compute_layout(SimLayout layout, int s, int window_w, int window_h) {
    if (s < 1) s = 1;
    LayoutResult r;
    r.size = natural_size(layout, s);
    const int x = (window_w - r.size.w) / 2;
    const int y = (window_h - r.size.h) / 2;
    switch (layout) {
    case SimLayout::Stacked:
        r.top = {true, x, y, s};
        r.bottom = {true, x + (kTopW - kBottomW) / 2 * s, y + kScreenH * s, s};
        break;
    case SimLayout::SideBySide:
        r.top = {true, x, y, s};
        r.bottom = {true, x + kTopW * s, y, s};
        break;
    case SimLayout::LargeTop: {
        const int b = small_scale(s);
        r.top = {true, x, y, s};
        r.bottom = {true, x + kTopW * s, y + kScreenH * s - kScreenH * b, b};
        break;
    }
    case SimLayout::TopOnly: r.top = {true, x, y, s}; break;
    case SimLayout::BottomOnly: r.bottom = {true, x, y, s}; break;
    }
    return r;
}

bool window_to_bottom(const LayoutResult& layout, float x, float y, bool clamp, float& bottom_x, float& bottom_y) {
    const Placement& b = layout.bottom;
    if (!b.visible) return false;
    float lx = (x - static_cast<float>(b.x)) / static_cast<float>(b.scale);
    float ly = (y - static_cast<float>(b.y)) / static_cast<float>(b.scale);
    const bool inside = lx >= 0 && ly >= 0 && lx < kBottomW && ly < kScreenH;
    if (!inside && !clamp) return false;
    if (clamp) {
        lx = lx < 0 ? 0 : (lx > kBottomW - 1 ? kBottomW - 1 : lx);
        ly = ly < 0 ? 0 : (ly > kScreenH - 1 ? kScreenH - 1 : ly);
    }
    bottom_x = lx;
    bottom_y = ly;
    return true;
}

SimLayout next_layout(SimLayout layout) {
    for (std::size_t i = 0; i < sizeof kNames / sizeof kNames[0]; ++i) {
        if (kNames[i].layout == layout) return kNames[(i + 1) % (sizeof kNames / sizeof kNames[0])].layout;
    }
    return SimLayout::Stacked;
}

const char* layout_id(SimLayout layout) { return names_of(layout).id; }
const char* layout_title(SimLayout layout) { return names_of(layout).title; }

bool parse_layout(const char* id, SimLayout& out) {
    if (id == nullptr) return false;
    for (const Names& n : kNames) {
        if (std::strcmp(n.id, id) == 0) {
            out = n.layout;
            return true;
        }
    }
    return false;
}

} // namespace nnh::platform::pc
