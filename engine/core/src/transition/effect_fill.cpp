// Fade, wipe, split and blinds (plan 023 "Geometry"): rectangles from ext(L) = (q · L + 32768) >> 16, with their
// Reveal::Continue forms. Integer-only; blinds use one division per call and an exact running remainder.

#include "transition/effect_geometry.hpp"

namespace nnh::detail {
namespace {

using transition::Edge;
using transition::Kind;
using transition::SplitAxis;

Edge opposite(Edge edge) {
    switch (edge) {
    case Edge::Left: return Edge::Right;
    case Edge::Right: return Edge::Left;
    case Edge::Top: return Edge::Bottom;
    case Edge::Bottom: return Edge::Top;
    }
    return edge;
}

bool across_x(Edge edge) { return edge == Edge::Left || edge == Edge::Right; }

/// A cover of extent e (> 0) growing from `edge` of the box (x, y, w, h).
void edge_rect(Edge edge, int x, int y, int w, int h, int e, const RectTarget& t) {
    if (e <= 0) return;
    switch (edge) {
    case Edge::Left: t.emit(x, y, e, h); break;
    case Edge::Right: t.emit(x + w - e, y, e, h); break;
    case Edge::Top: t.emit(x, y, w, e); break;
    case Edge::Bottom: t.emit(x, y + h - e, w, e); break;
    }
}

Edge fx_edge(const ResolvedEffect& fx) {
    const Edge edge = static_cast<Edge>(fx.dir);
    return fx.continued ? opposite(edge) : edge;
}

void wipe(const ResolvedEffect& fx, int q, int w, int h, const RectTarget& t) {
    const Edge edge = fx_edge(fx);
    edge_rect(edge, 0, 0, w, h, ext(q, across_x(edge) ? w : h), t);
}

void split(const ResolvedEffect& fx, int q, int w, int h, const RectTarget& t) {
    const bool lr = static_cast<SplitAxis>(fx.dir) == SplitAxis::LeftRight;
    const int length = lr ? w : h;
    const int e = ext(q, length);
    if (e <= 0) return;
    if (fx.continued) { // one centred band, shrinking to the middle
        const int start = (length - e) >> 1;
        if (lr) {
            t.emit(start, 0, e, h);
        } else {
            t.emit(0, start, w, e);
        }
        return;
    }
    const int a = (e + 1) >> 1; // from the left (top) edge
    const int b = e >> 1;       // from the right (bottom) edge
    edge_rect(lr ? Edge::Left : Edge::Top, 0, 0, w, h, a, t);
    edge_rect(lr ? Edge::Right : Edge::Bottom, 0, 0, w, h, b, t);
}

void blinds(const ResolvedEffect& fx, int q, int w, int h, const RectTarget& t) {
    const Edge edge = fx_edge(fx);
    const bool x_bars = across_x(edge);
    const int length = x_bars ? w : h;
    const int n = fx.bars;
    const int base = length / n; // the call's only division (with the remainder below)
    const int rem = length - base * n;
    int start = 0;
    int acc = 0; // (i · rem) mod n: bar i spans [⌊i·L/n⌋, ⌊(i+1)·L/n⌋)
    for (int i = 0; i < n; ++i) {
        int size = base;
        acc += rem;
        if (acc >= n) {
            acc -= n;
            ++size;
        }
        const int e = ext(q, size);
        if (x_bars) {
            edge_rect(edge, start, 0, size, h, e, t);
        } else {
            edge_rect(edge, 0, start, w, size, e, t);
        }
        start += size;
    }
}

} // namespace

void geometry_fill(const ResolvedEffect& fx, int q, int w, int h, const RectTarget& t) {
    if (q <= 0) return;
    switch (fx.kind) {
    case Kind::Fade:
        if (effect_color_q(fx, q).a > 0) t.emit(0, 0, w, h);
        break;
    case Kind::Wipe: wipe(fx, q, w, h, t); break;
    case Kind::Split: split(fx, q, w, h, t); break;
    case Kind::Blinds: blinds(fx, q, w, h, t); break;
    default: break;
    }
}

Color effect_color_q(const ResolvedEffect& fx, int q) {
    Color c = fx.color;
    if (fx.kind == Kind::Fade) {
        const int qc = q < 0 ? 0 : (q > kFullQ ? kFullQ : q);
        c.a = static_cast<std::uint8_t>((static_cast<std::uint32_t>(qc) * 255u + 32768u) >> 16);
    } else {
        c.a = 255;
    }
    return c;
}

} // namespace nnh::detail
