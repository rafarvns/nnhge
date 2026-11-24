// Building distance fields (spec 025 FR-16, FR-17): Dijkstra from the goals over reversed steps — an agent at p steps
// into the settled cell c, paying step x w(c), so a settled cell's two candidates d + S·w(c) and d + D·w(c) are computed
// once and each relaxation is a load, a compare and a store; the diagonal rule is symmetric. A min-heap keyed
// (d, padded index), lazily deleted, compacted when full (never needed in practice: one live entry per open cell).

#include <climits>

#include "pathfinding/field_internal.hpp"
#include "pathfinding/neighbors.hpp"
#include "pathfinding/path_check.hpp"

namespace nnh {
namespace detail {
namespace {

bool before(const FieldEntry& a, const FieldEntry& b) {
    return a.d < b.d || (a.d == b.d && a.cell < b.cell);
}

void heap_push(FieldBlock& f, FieldEntry e) {
    FieldEntry* h = f.heap;
    int i = f.heap_size++;
    while (i > 0) {
        const int parent = (i - 1) >> 1;
        if (!before(e, h[parent])) break;
        h[i] = h[parent];
        i = parent;
    }
    h[i] = e;
}

void sift_down(FieldBlock& f, int i, FieldEntry e) {
    FieldEntry* h = f.heap;
    const int n = f.heap_size;
    while (true) {
        int c = 2 * i + 1;
        if (c >= n) break;
        if (c + 1 < n && before(h[c + 1], h[c])) ++c;
        if (!before(h[c], e)) break;
        h[i] = h[c];
        i = c;
    }
    h[i] = e;
}

/// Cold: drops stale entries and heapifies.
void compact(FieldBlock& f) {
    const std::uint32_t* back = f.back();
    int kept = 0;
    for (int i = 0; i < f.heap_size; ++i) {
        if (f.heap[i].d == back[f.heap[i].cell]) f.heap[kept++] = f.heap[i];
    }
    f.heap_size = kept;
    for (int i = (kept >> 1) - 1; i >= 0; --i) sift_down(f, i, f.heap[i]);
}

bool side_ok(Diagonals mode, std::uint32_t a, std::uint32_t b) {
    if (mode == Diagonals::Always) return true;
    if (mode == Diagonals::AtLeastOneWalkable) return (a | b) != 0;
    return a != 0 && b != 0;
}

void finish(FieldBlock& f) {
    f.building = false;
    f.stale = f.map->version != f.version;
    if (f.double_buffered) f.front = 1 - f.front;
    f.ready = true;
    f.ans_stride = f.stride;
    f.ans_w = f.map_w;
    f.ans_h = f.map_h;
    field_release_map(f);
}

bool goals_ok(const FieldBlock& f, const PathMapBlock* m, const CellPos* goals, int count) {
    if (m == nullptr) {
        path_misuse("DistanceField::begin on an empty map");
        return false;
    }
    if (m->width > f.width || m->height > f.height) {
        path_misuse("DistanceField::begin: the map is larger than the field");
        return false;
    }
    if (!path_buffer_ok(goals, count, "DistanceField::begin (goals)")) return false;
    for (int i = 0; i < count; ++i) cell_ok(m->width, m->height, goals[i].x, goals[i].y, "DistanceField::begin (goal)");
    return true;
}

} // namespace

bool field_run(FieldBlock& f, int budget) {
    const std::uint8_t* cells = f.map->cells;
    std::uint32_t* back = f.back();
    const int* off = f.offsets;
    const bool diagonals = f.mode != Diagonals::Never;
    while (true) {
        if (f.heap_size == 0) {
            finish(f);
            return true;
        }
        if (budget == 0) return false;
        const FieldEntry e = f.heap[0];
        --f.heap_size;
        if (f.heap_size > 0) sift_down(f, 0, f.heap[f.heap_size]);
        if (e.d != back[e.cell]) continue; // stale: skipped, not counted
        ++f.settled;
        --budget;
        const int c = static_cast<int>(e.cell);
        const std::uint32_t w = cells[c];
        const std::uint32_t ds = e.d + f.straight * w;
        const std::uint32_t dd = e.d + f.diagonal * w;
        const std::uint32_t wo[4] = {cells[c + off[0]], cells[c + 1], cells[c + off[2]], cells[c - 1]};
        for (int k = 0; k < (diagonals ? 8 : 4); ++k) {
            const int p = c + off[k];
            if (cells[p] == 0) continue;
            if (k >= 4 && !side_ok(f.mode, wo[kSideA[k - 4]], wo[kSideB[k - 4]])) continue;
            const std::uint32_t cand = k < 4 ? ds : dd;
            if (cand > f.max_cost || cand >= back[p]) continue;
            back[p] = cand;
            if (f.heap_size >= f.heap_capacity) compact(f);
            heap_push(f, {cand, static_cast<std::uint32_t>(p)});
        }
    }
}

} // namespace detail

void DistanceField::begin(const PathMap& map, const CellPos* goals, int count, std::uint32_t max_cost) {
    detail::FieldBlock* f = block_;
    if (f == nullptr) {
        detail::path_misuse("DistanceField::begin on an empty field");
        return;
    }
    cancel();
    detail::PathMapBlock* m = detail::PathAccess::map(map);
    if (!detail::goals_ok(*f, m, goals, count)) return;
    f->map = m;
    f->stride = m->stride;
    f->map_w = m->width;
    f->map_h = m->height;
    f->version = m->version;
    f->straight = static_cast<std::uint32_t>(m->straight);
    f->diagonal = static_cast<std::uint32_t>(m->diagonal);
    f->mode = m->diagonals;
    f->max_cost = max_cost;
    f->settled = 0;
    f->heap_size = 0;
    detail::neighbour_offsets(f->stride, f->offsets);
    if (!f->double_buffered) f->ready = false; // its only array is rewritten
    std::uint32_t* back = f->back();
    const int padded = f->stride * (m->height + 2);
    for (int i = 0; i < padded; ++i) back[i] = kUnreachable;
    for (int i = 0; i < count; ++i) {
        const CellPos g = goals[i];
        if (g.x < 0 || g.y < 0 || g.x >= m->width || g.y >= m->height) continue; // misuse, reported: ignored
        const int c = detail::padded_index(f->stride, g.x, g.y);
        if (m->cells[c] == 0 || back[c] == 0) continue; // blocked goals ignored, duplicates harmless
        back[c] = 0;
        detail::heap_push(*f, {0, static_cast<std::uint32_t>(c)});
    }
    f->building = true;
    f->holds_map = true;
    ++m->users;
}

bool DistanceField::step(int max_cells) {
    detail::FieldBlock* f = block_;
    if (f == nullptr) {
        detail::path_misuse("DistanceField::step on an empty field");
        return true;
    }
    if (max_cells < 0) {
        detail::path_misuse("DistanceField::step: negative step size");
        return !f->building;
    }
    if (!f->building) return true;
    if (f->map->doomed) { // the map was destroyed during the build: it ends, the last finished one answers
        cancel();
        return true;
    }
    return detail::field_run(*f, max_cells);
}

void DistanceField::build(const PathMap& map, const CellPos* goals, int count, std::uint32_t max_cost) {
    begin(map, goals, count, max_cost);
    if (block_ != nullptr && block_->building) step(INT_MAX);
}

void DistanceField::cancel() {
    if (block_ == nullptr || !block_->building) return;
    block_->building = false;
    detail::field_release_map(*block_);
}

} // namespace nnh
