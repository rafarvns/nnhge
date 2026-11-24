// Field queries (spec 025 FR-18): distance, next (the neighbour minimising step x weight + distance, ties in the fixed
// neighbour order, under the map's diagonal rule; no stored directions), next_point. Reads only.

#include "pathfinding/field_internal.hpp"
#include "pathfinding/neighbors.hpp"
#include "pathfinding/path_check.hpp"

namespace nnh {

std::uint32_t DistanceField::distance(CellPos cell) const {
    const detail::FieldBlock* f = block_;
    if (f == nullptr || !f->ready) return kUnreachable;
    if (!detail::cell_ok(f->ans_w, f->ans_h, cell.x, cell.y, "DistanceField::distance")) return kUnreachable;
    return f->answer()[detail::padded_index(f->ans_stride, cell.x, cell.y)];
}

bool DistanceField::next(const PathMap& map, CellPos at, CellPos& out) const {
    const detail::FieldBlock* f = block_;
    const detail::PathMapBlock* m = detail::PathAccess::map(map);
    if (f == nullptr || !f->ready) return false;
    if (m == nullptr || m->width != f->ans_w || m->height != f->ans_h) {
        detail::path_misuse("DistanceField::next: not the map the field was built over");
        return false;
    }
    if (!detail::cell_ok(m->width, m->height, at.x, at.y, "DistanceField::next")) return false;
    const std::uint32_t* d = f->answer();
    const int c = detail::padded_index(f->ans_stride, at.x, at.y);
    if (d[c] == 0 || d[c] == kUnreachable) return false;
    const std::uint8_t* cells = m->cells;
    int off[8];
    detail::neighbour_offsets(f->ans_stride, off);
    const std::uint32_t wo[4] = {cells[c + off[0]], cells[c + 1], cells[c + off[2]], cells[c - 1]};
    const Diagonals mode = m->diagonals;
    const std::uint32_t steps[2] = {static_cast<std::uint32_t>(m->straight), static_cast<std::uint32_t>(m->diagonal)};
    std::uint32_t best = kUnreachable;
    int best_k = -1;
    for (int k = 0; k < detail::direction_count(mode); ++k) {
        const int n = c + off[k];
        const std::uint32_t w = cells[n];
        if (w == 0 || d[n] == kUnreachable) continue;
        if (k >= 4 && !detail::diagonal_allowed(mode, wo[detail::kSideA[k - 4]] != 0, wo[detail::kSideB[k - 4]] != 0)) {
            continue;
        }
        const std::uint32_t cost = steps[k >= 4 ? 1 : 0] * w + d[n];
        if (cost < best) {
            best = cost;
            best_k = k;
        }
    }
    if (best_k < 0) return false;
    out = {at.x + detail::kDirDx[best_k], at.y + detail::kDirDy[best_k]};
    return true;
}

bool DistanceField::next_point(const PathMap& map, Vec2 at, Vec2& out) const {
    const detail::PathMapBlock* m = detail::PathAccess::map(map);
    if (m == nullptr || !detail::path_point_ok(at, "DistanceField::next_point")) return false;
    const CellPos c = map.cell_at(at);
    if (!map.contains(c)) return false;
    CellPos n;
    if (!next(map, c, n)) return false;
    out = detail::path_cell_center(*m, n);
    return true;
}

} // namespace nnh
