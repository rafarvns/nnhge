// Zoomed tile edges (see zoom_edges.hpp).

#include "tilemap/zoom_edges.hpp"

namespace nnh::detail {
namespace {

/// ceil(a / b) for b > 0.
std::int64_t ceil_div(std::int64_t a, std::int64_t b) {
    const std::int64_t q = a / b;
    return (a % b != 0 && a > 0) ? q + 1 : q;
}

int clamp_cells(std::int64_t v, int cells) { return v < 0 ? 0 : (v > cells ? cells : static_cast<int>(v)); }

} // namespace

void zoom_axis_range(int origin, int tile, int zq, int screen, int cells, int& first, int& last) {
    const std::int64_t step = static_cast<std::int64_t>(tile) * zq; // one cell, in 1/65536 pixels
    // Cell i is visible when edge(i + 1) > 0, i.e. (i + 1)·step + 32768 ≥ (1 − origin)·65536 ...
    const std::int64_t need_end = (static_cast<std::int64_t>(1) - origin) * 65536 - 32768;
    first = clamp_cells(ceil_div(need_end, step) - 1, cells);
    // ... and edge(i) < screen, i.e. i·step + 32768 < (screen − origin)·65536.
    const std::int64_t below = (static_cast<std::int64_t>(screen) - origin) * 65536 - 32768;
    last = below <= 0 ? 0 : clamp_cells(ceil_div(below, step), cells);
    if (last < first) last = first;
}

void zoom_edges(int origin, int tile, int zq, int first, int count, std::int16_t* out) {
    for (int k = 0; k <= count; ++k) out[k] = static_cast<std::int16_t>(origin + zoom_edge(first + k, tile, zq));
}

} // namespace nnh::detail
