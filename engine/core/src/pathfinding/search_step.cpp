// The expansion loop (spec 025 FR-9, FR-10, FR-12, FR-13, D40): one template per diagonal mode, the neighbour loop
// unrolled in the fixed order N, E, S, W, NE, SE, SW, NW with the orthogonal bytes loaded once and reused for the
// diagonal rule; the limit checked before each pop; stale entries skipped without counting; a parent changed only on
// a strictly lower g; closed cells never reopened; partial tracking at every g assignment. No division, no calls in
// the loop except the cold compaction.

#include "pathfinding/heuristic.hpp"
#include "pathfinding/search_internal.hpp"

namespace nnh::detail {
namespace {

bool entry_live(const void* context, const OpenEntry& e) {
    const auto& s = *static_cast<const PathSearchBlock*>(context);
    const SearchRecord& r = s.record[record_index(s, e.cell)];
    return (r.meta & kMetaClosed) == 0 && r.g == e.f - e.h;
}

template <Diagonals M>
class Expander {
public:
    explicit Expander(PathSearchBlock& s)
        : s_(s), cells_(s.map->cells), rec_(s.record), stamp_(s.stamp << kStampShift) {}

    /// Relaxes every neighbour of the closed cell `c` (padded index `i`, cost `g`).
    void expand(std::uint32_t c, int i, std::uint32_t g) {
        const int* off = s_.offsets;
        const std::uint32_t wn = cells_[i + off[0]];
        const std::uint32_t we = cells_[i + 1];
        const std::uint32_t ws = cells_[i + off[2]];
        const std::uint32_t ww = cells_[i - 1];
        const std::uint32_t st = s_.straight;
        const int x = packed_x(c);
        const int y = packed_y(c);
        if (wn != 0) relax(i + off[0], c, x, y, 0, g + st * wn);
        if (we != 0) relax(i + 1, c, x, y, 1, g + st * we);
        if (ws != 0) relax(i + off[2], c, x, y, 2, g + st * ws);
        if (ww != 0) relax(i - 1, c, x, y, 3, g + st * ww);
        if constexpr (M != Diagonals::Never) {
            const std::uint32_t dg = s_.diagonal;
            const std::uint32_t wne = cells_[i + off[4]];
            const std::uint32_t wse = cells_[i + off[5]];
            const std::uint32_t wsw = cells_[i + off[6]];
            const std::uint32_t wnw = cells_[i + off[7]];
            if (wne != 0 && allowed(wn, we)) relax(i + off[4], c, x, y, 4, g + dg * wne);
            if (wse != 0 && allowed(we, ws)) relax(i + off[5], c, x, y, 5, g + dg * wse);
            if (wsw != 0 && allowed(ws, ww)) relax(i + off[6], c, x, y, 6, g + dg * wsw);
            if (wnw != 0 && allowed(ww, wn)) relax(i + off[7], c, x, y, 7, g + dg * wnw);
        }
    }

private:
    static bool allowed(std::uint32_t a, std::uint32_t b) {
        if constexpr (M == Diagonals::Always) {
            return true;
        } else if constexpr (M == Diagonals::AtLeastOneWalkable) {
            return (a | b) != 0;
        } else {
            return a != 0 && b != 0;
        }
    }

    void relax(int n, std::uint32_t c, int x, int y, int d, std::uint32_t ng) {
        SearchRecord& r = rec_[n];
        if ((r.meta >> kStampShift) == (stamp_ >> kStampShift)) {
            if ((r.meta & kMetaClosed) != 0 || ng >= r.g) return; // closed, or not strictly lower
        }
        r.g = ng;
        r.meta = stamp_ | static_cast<std::uint32_t>(d);
        const int nx = x + kDirDx[d];
        const int ny = y + kDirDy[d];
        const int dx = nx > s_.tx ? nx - s_.tx : s_.tx - nx;
        const int dy = ny > s_.ty ? ny - s_.ty : s_.ty - ny;
        const std::uint32_t hu = path_heuristic(M, s_.straight, s_.diagonal, dx, dy);
        const std::uint32_t h = s_.scale == 16 ? hu : scale_heuristic(hu, s_.scale);
        const std::uint32_t nc = c + kPackedDelta[d];
        if (s_.partial) track(nc, hu, ng);
        if (s_.heap.full()) open_compact(s_.heap, entry_live, &s_);
        s_.heap.offer({h, ng + h, s_.seq++, nc});
    }

    /// FR-13: the reached cell with the lowest unscaled h, then the lower g, then the lower row-major index (the
    /// packed cell's order).
    void track(std::uint32_t nc, std::uint32_t hu, std::uint32_t ng) {
        if (hu < s_.best_h || (hu == s_.best_h && (ng < s_.best_g || (ng == s_.best_g && nc < s_.best)))) {
            s_.best = nc;
            s_.best_h = hu;
            s_.best_g = ng;
        }
    }

    PathSearchBlock& s_;
    const std::uint8_t* cells_;
    SearchRecord* rec_;
    std::uint32_t stamp_;
};

template <Diagonals M>
bool run(PathSearchBlock& s, int budget) {
    Expander<M> ex(s);
    while (true) {
        if (s.result.expanded >= s.limit) {
            search_finish(s, PathStatus::LimitReached);
            return true;
        }
        if (budget == 0) return false;
        OpenEntry e;
        int i = 0;
        while (true) {
            if (!s.heap.take(e)) {
                search_finish(s, PathStatus::NoPath);
                return true;
            }
            i = record_index(s, e.cell);
            const SearchRecord& r = s.record[i];
            if ((r.meta & kMetaClosed) == 0 && r.g == e.f - e.h) break; // else stale: skipped, not counted
        }
        s.record[i].meta |= kMetaClosed;
        ++s.result.expanded;
        --budget;
        if (e.cell == s.to) {
            search_finish(s, PathStatus::Found);
            return true;
        }
        ex.expand(e.cell, i, s.record[i].g);
    }
}

} // namespace

bool search_run(PathSearchBlock& s, int budget) {
    switch (s.mode) {
    case Diagonals::Never: return run<Diagonals::Never>(s, budget);
    case Diagonals::Always: return run<Diagonals::Always>(s, budget);
    case Diagonals::AtLeastOneWalkable: return run<Diagonals::AtLeastOneWalkable>(s, budget);
    default: return run<Diagonals::OnlyIfNoObstacles>(s, budget);
    }
}

} // namespace nnh::detail
