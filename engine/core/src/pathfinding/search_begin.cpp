// Starting and ending searches (spec 025 FR-8, FR-11..FR-14, FR-23): begin's checks and the start and goal rules,
// step's dispatch (a map destroyed meanwhile ends the search Invalid), find, cancel, and the finish (status, stale,
// cost, path length).

#include <climits>

#include "pathfinding/heuristic.hpp"
#include "pathfinding/path_check.hpp"
#include "pathfinding/search_internal.hpp"

namespace nnh {
namespace detail {
namespace {

/// A new stamp: nothing reached; the wrap zeroes every meta once.
void next_stamp(PathSearchBlock& s) {
    if (s.stamp >= kMaxStamp) {
        for (int i = 0; i < s.records; ++i) s.record[i].meta = 0;
        s.stamp = 0;
    }
    ++s.stamp;
}

int path_length(const PathSearchBlock& s) {
    int n = 1;
    for (std::uint32_t c = s.end; c != s.from; ++n) {
        c -= kPackedDelta[s.record[record_index(s, c)].meta & kMetaDir];
    }
    return n;
}

bool options_ok(const PathOptions& o) {
    if (o.max_expanded < 0) {
        path_misuse("PathSearcher::begin: options.max_expanded is negative");
        return false;
    }
    if (o.heuristic_scale < 16 || o.heuristic_scale > 64) {
        path_misuse("PathSearcher::begin: options.heuristic_scale not 16..64");
        return false;
    }
    return true;
}

/// begin()'s checks; false: the result is Invalid.
bool begin_ok(const PathSearchBlock& s, const PathMapBlock* m, CellPos from, CellPos to, const PathOptions& o) {
    if (m == nullptr) {
        path_misuse("PathSearcher::begin on an empty map");
        return false;
    }
    if (m->width > s.width || m->height > s.height) {
        path_misuse("PathSearcher::begin: the map is larger than the searcher");
        return false;
    }
    return cell_ok(m->width, m->height, from.x, from.y, "PathSearcher::begin (from)") &&
           cell_ok(m->width, m->height, to.x, to.y, "PathSearcher::begin (to)") && options_ok(o);
}

} // namespace

void search_finish(PathSearchBlock& s, PathStatus status) {
    if (s.partial && (status == PathStatus::NoPath || status == PathStatus::LimitReached)) {
        status = PathStatus::Partial;
        s.end = s.best;
    } else if (status == PathStatus::Found) {
        s.end = s.to;
    }
    s.result.status = status;
    s.result.stale = s.map != nullptr && s.map->version != s.version;
    if (status == PathStatus::Found || status == PathStatus::Partial) {
        s.result.cost = s.record[record_index(s, s.end)].g;
        s.result.cells = path_length(s);
    } else {
        s.result.cost = 0;
        s.result.cells = 0;
    }
    search_release_map(s);
}

} // namespace detail

void PathSearcher::begin(const PathMap& map, CellPos from, CellPos to, const PathOptions& options) {
    detail::PathSearchBlock* s = block_;
    if (s == nullptr) {
        detail::path_misuse("PathSearcher::begin on an empty searcher");
        return;
    }
    detail::search_release_map(*s); // cancels a running search
    s->result = {};
    detail::PathMapBlock* m = detail::PathAccess::map(map);
    if (!detail::begin_ok(*s, m, from, to, options)) {
        s->map_w = s->map_h = 0; // inspection answers nothing
        s->result.status = PathStatus::Invalid;
        return;
    }
    s->map = m;
    s->stride = m->stride;
    s->map_w = m->width;
    s->map_h = m->height;
    s->version = m->version;
    s->straight = static_cast<std::uint32_t>(m->straight);
    s->diagonal = static_cast<std::uint32_t>(m->diagonal);
    s->mode = m->diagonals;
    s->scale = options.heuristic_scale;
    s->limit = options.max_expanded > 0 ? options.max_expanded : m->width * m->height;
    s->partial = options.partial;
    s->from = detail::pack_cell(from.x, from.y);
    s->to = detail::pack_cell(to.x, to.y);
    s->tx = to.x;
    s->ty = to.y;
    s->seq = 0;
    detail::neighbour_offsets(s->stride, s->offsets);
    detail::next_stamp(*s);
    s->heap.reset();
    detail::SearchRecord& start = s->record[detail::padded_index(s->stride, from.x, from.y)];
    start.g = 0;
    start.meta = s->stamp << detail::kStampShift;
    s->end = s->from;
    if (from == to) { // FR-11: found with one cell, walkable or not
        s->result.status = PathStatus::Found;
        s->result.cells = 1;
        return;
    }
    if (m->cells[detail::padded_index(s->stride, to.x, to.y)] == 0 && !options.partial) {
        s->result.status = PathStatus::NoPath; // a blocked goal: no flood
        return;
    }
    const int dx = from.x > to.x ? from.x - to.x : to.x - from.x;
    const int dy = from.y > to.y ? from.y - to.y : to.y - from.y;
    const std::uint32_t hu = detail::path_heuristic(s->mode, s->straight, s->diagonal, dx, dy);
    const std::uint32_t h = detail::scale_heuristic(hu, s->scale);
    s->best = s->from;
    s->best_h = hu;
    s->best_g = 0;
    s->heap.push({h, h, s->seq++, s->from});
    s->result.status = PathStatus::Running;
    s->holds_map = true;
    ++m->users;
}

bool PathSearcher::step(int max_expanded) {
    detail::PathSearchBlock* s = block_;
    if (s == nullptr) {
        detail::path_misuse("PathSearcher::step on an empty searcher");
        return true;
    }
    if (max_expanded < 0) {
        detail::path_misuse("PathSearcher::step: negative step size");
        return s->result.status != PathStatus::Running;
    }
    if (s->result.status == PathStatus::None) {
        detail::path_misuse("PathSearcher::step without begin()");
        return true;
    }
    if (s->result.status != PathStatus::Running) return true;
    if (s->map->doomed) { // the map was destroyed during the search
        s->result = {PathStatus::Invalid, false, 0, 0, s->result.expanded};
        detail::search_release_map(*s);
        return true;
    }
    return detail::search_run(*s, max_expanded);
}

PathResult PathSearcher::find(const PathMap& map, CellPos from, CellPos to, const PathOptions& options) {
    begin(map, from, to, options);
    if (block_ != nullptr && block_->result.status == PathStatus::Running) step(INT_MAX);
    return result();
}

void PathSearcher::cancel() {
    if (block_ == nullptr) return;
    detail::search_release_map(*block_);
    block_->result = {};
}

bool PathSearcher::running() const { return block_ != nullptr && block_->result.status == PathStatus::Running; }

PathResult PathSearcher::result() const { return block_ != nullptr ? block_->result : PathResult{}; }

} // namespace nnh
