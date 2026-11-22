#include "easing_gallery.hpp"

#include <nnh/gfx.hpp>
#include <nnh/tween.hpp>

namespace {

// At most 8 characters: a cell is 80 pixels wide and the built-in font 8 pixels per character.
constexpr const char* kNames[nnh::kEaseCount] = {"linear",   "quad in",  "quad out", "quad io",  "cubic in",
                                                 "cubicout", "cubic io", "sine in",  "sine out", "sine io",
                                                 "back in",  "back out", "back io",  "elas in",  "elas out",
                                                 "elas io",  "bnce in",  "bnce out", "bnce io"};

constexpr int kColumns = 5;
constexpr float kCellW = 80, kCellH = 60;
constexpr float kPlotW = 60, kPlotH = 26; // the curve's 0..1 box; overshoot draws outside it
constexpr int kSegments = 20;

void draw_cell(int index, float phase) {
    const auto curve = static_cast<nnh::Ease>(index);
    const float x0 = 10 + (index % kColumns) * kCellW;
    const float y0 = (index / kColumns) * kCellH;
    const float base = y0 + 46; // y of value 0
    const nnh::Screen top = nnh::Screen::Top;
    nnh::gfx::draw_text(top, {x0 - 6, y0 + 2}, kNames[index], {.color = nnh::rgb(200, 210, 230)});
    nnh::gfx::draw_line(top, {x0, base}, {x0 + kPlotW, base}, nnh::rgb(70, 80, 110));
    nnh::gfx::draw_line(top, {x0, base - kPlotH}, {x0 + kPlotW, base - kPlotH}, nnh::rgb(70, 80, 110));
    nnh::Vec2 last{x0, base};
    for (int s = 1; s <= kSegments; ++s) {
        const float t = static_cast<float>(s) / kSegments;
        const nnh::Vec2 point{x0 + t * kPlotW, base - nnh::ease(curve, t) * kPlotH};
        nnh::gfx::draw_line(top, last, point, nnh::rgb(120, 200, 255));
        last = point;
    }
    const float v = nnh::ease(curve, phase);
    nnh::gfx::fill_rect(top, {x0 + phase * kPlotW - 2, base - v * kPlotH - 2, 5, 5}, nnh::colors::yellow);
    nnh::gfx::fill_rect(top, {x0 + v * kPlotW - 1, base + 6, 3, 4},
                        nnh::rgb(255, 140, 80)); // the eased position
}

} // namespace

void draw_easing_gallery(float phase) {
    for (int i = 0; i < nnh::kEaseCount; ++i) {
        draw_cell(i, phase);
    }
}
