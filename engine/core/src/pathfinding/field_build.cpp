// Building distance fields (spec 025 FR-16, FR-17): Dijkstra from the goals over reversed steps — an agent at p steps
// into the settled cell c, paying step x w(c), so a settled cell's two candidates d + S·w(c) and d + D·w(c) are computed
// once and each relaxation is a load, a compare and a store; the diagonal rule is symmetric. One template per diagonal
// mode with the neighbour loop unrolled (T10.3), the open set in field_heap.hpp. Each relaxation that sets or ties a
// cell's distance also keeps next()'s answer for it: 1 + the direction from p back to c, the lowest on ties (every c
// whose candidate equals d(p) is settled before p, so the neighbour-order argmin is complete when p settles). begin()
// clears only the step bytes: a distance word counts once its cell's step byte is set (field_internal.hpp).

#include <climits>

#include "pathfinding/field_internal.hpp"
#include "pathfinding/neighbors.hpp"
#include "pathfinding/path_check.hpp"

namespace nnh {
namespace detail {
namespace {

/// Cold: drops stale entries and heapifies.
void compact(FieldBlock& f) {
    const std::uint32_t* back = f.back();
    int kept = 0;
    for (int i = 0; i < f.heap_size; ++i) {
        const FieldKey k = f.heap[i];
        if (field_key_d(k) == back[field_key_cell(k)]) f.heap[kept++] = k;
    }
    f.heap_size = kept;
    field_heapify(f.heap, kept);
}

void finish(FieldBlock& f) {
    f.building = false;
    f.stale = f.map->version != f.version;
    if (f.double_buffered) f.front = 1 - f.front;
    f.ready = true;
    f.ans_stride = f.stride;
    f.ans_w = f.map_w;
    f.ans_h = f.map_h;
    f.ans_serial = f.map->serial;
    f.ans_version = f.version;
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

/// The diagonal rule on `open` (bit i: the settled cell's orthogonal neighbour i walkable) for side cells `sides`.
template <Diagonals M>
NNH_PATH_INLINE bool side_ok(std::uint32_t open, std::uint32_t sides) {
    if constexpr (M == Diagonals::Always) {
        return true;
    } else if constexpr (M == Diagonals::AtLeastOneWalkable) {
        return (open & sides) != 0;
    } else {
        return (open & sides) == sides;
    }
}

template <Diagonals M>
bool run(FieldBlock& f, int budget) {
    const std::uint8_t* cells = f.map->cells;
    std::uint32_t* back = f.back();
    std::uint8_t* steps = f.back_steps();
    FieldKey* heap = f.heap;
    int n = f.heap_size;
    int settled = f.settled;
    const int s = f.stride;
    const std::uint32_t straight = f.straight;
    const std::uint32_t diagonal = f.diagonal;
    const std::uint32_t cap = f.max_cost;
    // p entered with `cand` from the settled cell, `from` = 1 + the direction from p to that cell.
    auto relax = [&](int p, std::uint32_t cand, std::uint32_t from) NNH_PATH_INLINE_LAMBDA {
        const std::uint32_t old = steps[p] != 0 ? back[p] : kUnreachable; // 0: not reached by this build
        if (cand < old) {
            back[p] = cand;
            steps[p] = static_cast<std::uint8_t>(from);
            if (n >= f.heap_capacity) {
                f.heap_size = n;
                compact(f);
                n = f.heap_size;
            }
            field_sift_up(heap, n++, field_key(cand, static_cast<std::uint32_t>(p)));
        } else if (cand == old && from < steps[p]) {
            steps[p] = static_cast<std::uint8_t>(from);
        }
    };
    const int stop = budget > INT_MAX - settled ? INT_MAX : settled + budget;
    bool done = false;
    while (true) {
        if (n == 0) {
            done = true;
            break;
        }
        if (settled == stop) break;
        const FieldKey top = heap[0];
        field_pop(heap, n--);
        const std::uint32_t d = field_key_d(top);
        const int c = static_cast<int>(field_key_cell(top));
        if (d != back[c]) continue; // stale: skipped, not counted
        ++settled;
        const std::uint32_t w = cells[c];
        // Bits 0..3: the N, E, S, W neighbours are walkable.
        const std::uint32_t open = (cells[c - s] != 0 ? 1u : 0u) | (cells[c + 1] != 0 ? 2u : 0u) |
                                   (cells[c + s] != 0 ? 4u : 0u) | (cells[c - 1] != 0 ? 8u : 0u);
        const std::uint32_t ds = d + straight * w;
        // `from`: N's cell sees c to its S (direction 2), E's to its W (3), S's to its N (0), W's to its E (1).
        if (ds <= cap) {
            if ((open & 1u) != 0) relax(c - s, ds, 1 + 2);
            if ((open & 2u) != 0) relax(c + 1, ds, 1 + 3);
            if ((open & 4u) != 0) relax(c + s, ds, 1 + 0);
            if ((open & 8u) != 0) relax(c - 1, ds, 1 + 1);
        }
        if constexpr (M != Diagonals::Never) {
            const std::uint32_t dd = d + diagonal * w;
            // NE's cell sees c to its SW (6), SE's to its NW (7), SW's to its NE (4), NW's to its SE (5).
            if (dd <= cap) {
                if (cells[c - s + 1] != 0 && side_ok<M>(open, 1u | 2u)) relax(c - s + 1, dd, 1 + 6);
                if (cells[c + s + 1] != 0 && side_ok<M>(open, 2u | 4u)) relax(c + s + 1, dd, 1 + 7);
                if (cells[c + s - 1] != 0 && side_ok<M>(open, 4u | 8u)) relax(c + s - 1, dd, 1 + 4);
                if (cells[c - s - 1] != 0 && side_ok<M>(open, 8u | 1u)) relax(c - s - 1, dd, 1 + 5);
            }
        }
    }
    f.heap_size = n;
    f.settled = settled;
    if (done) finish(f);
    return done;
}

} // namespace

bool field_run(FieldBlock& f, int budget) {
    switch (f.mode) {
    case Diagonals::Never: return run<Diagonals::Never>(f, budget);
    case Diagonals::Always: return run<Diagonals::Always>(f, budget);
    case Diagonals::AtLeastOneWalkable: return run<Diagonals::AtLeastOneWalkable>(f, budget);
    default: return run<Diagonals::OnlyIfNoObstacles>(f, budget);
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
    if (!f->double_buffered) f->ready = false; // its only array is rewritten
    std::uint32_t* back = f->back();
    std::uint8_t* steps = f->back_steps();
    const int padded = f->stride * (m->height + 2);
    for (int i = 0; i < padded; ++i) steps[i] = 0; // nothing reached: the distances aren't touched (T10.3)
    for (int i = 0; i < count; ++i) {
        const CellPos g = goals[i];
        if (g.x < 0 || g.y < 0 || g.x >= m->width || g.y >= m->height) continue; // misuse, reported: ignored
        const int c = detail::padded_index(f->stride, g.x, g.y);
        if (m->cells[c] == 0 || steps[c] != 0) continue; // blocked goals ignored, duplicates harmless
        back[c] = 0;
        steps[c] = detail::kGoalStep;
        detail::field_sift_up(f->heap, f->heap_size++, detail::field_key(0, static_cast<std::uint32_t>(c)));
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
