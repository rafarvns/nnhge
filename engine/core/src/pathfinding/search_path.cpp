// Reading a search (spec 025 FR-8, FR-20): the path walked back from its end through the stored directions (no
// division: coordinates move by the direction's deltas), written start first; inspection of the records.

#include "pathfinding/path_check.hpp"
#include "pathfinding/search_internal.hpp"

namespace nnh {
namespace {

bool has_path(const detail::PathSearchBlock* s) {
    return s != nullptr && (s->result.status == PathStatus::Found || s->result.status == PathStatus::Partial);
}

/// Calls put(position, cell) for every path cell, from the end back to the start, positions total - 1 .. 0.
template <typename Put>
void walk_back(const detail::PathSearchBlock& s, Put put) {
    int x = detail::packed_x(s.end);
    int y = detail::packed_y(s.end);
    const int fx = detail::packed_x(s.from);
    const int fy = detail::packed_y(s.from);
    for (int pos = s.result.cells - 1; pos >= 0; --pos) {
        put(pos, CellPos{x, y});
        if (x == fx && y == fy) break;
        const std::uint32_t d = s.record[detail::padded_index(s.stride, x, y)].meta & detail::kMetaDir;
        x -= detail::kDirDx[d];
        y -= detail::kDirDy[d];
    }
}

/// The record of `cell` if the last search reached it, else null (outside the map: misuse).
const detail::SearchRecord* reached_record(const detail::PathSearchBlock* s, CellPos cell, const char* who) {
    if (s == nullptr || s->map_w == 0) return nullptr;
    if (!detail::cell_ok(s->map_w, s->map_h, cell.x, cell.y, who)) return nullptr;
    const detail::SearchRecord& r = s->record[detail::padded_index(s->stride, cell.x, cell.y)];
    return (r.meta >> detail::kStampShift) == s->stamp ? &r : nullptr;
}

} // namespace

int PathSearcher::path(CellPos* out, int capacity) const {
    if (!detail::path_buffer_ok(out, capacity, "PathSearcher::path") || !has_path(block_)) return 0;
    walk_back(*block_, [&](int pos, CellPos c) {
        if (pos < capacity) out[pos] = c;
    });
    return block_->result.cells;
}

int PathSearcher::path_points(const PathMap& map, Vec2* out, int capacity) const {
    const detail::PathMapBlock* m = detail::PathAccess::map(map);
    if (m == nullptr) {
        detail::path_misuse("PathSearcher::path_points on an empty map");
        return 0;
    }
    if (!detail::path_buffer_ok(out, capacity, "PathSearcher::path_points") || !has_path(block_)) return 0;
    walk_back(*block_, [&](int pos, CellPos c) {
        if (pos < capacity) out[pos] = detail::path_cell_center(*m, c);
    });
    return block_->result.cells;
}

bool PathSearcher::reached(CellPos cell) const {
    return reached_record(block_, cell, "PathSearcher::reached") != nullptr;
}

bool PathSearcher::expanded(CellPos cell) const {
    const detail::SearchRecord* r = reached_record(block_, cell, "PathSearcher::expanded");
    return r != nullptr && (r->meta & detail::kMetaClosed) != 0;
}

std::uint32_t PathSearcher::cost_to(CellPos cell) const {
    const detail::SearchRecord* r = reached_record(block_, cell, "PathSearcher::cost_to");
    return r != nullptr ? r->g : kUnreachable;
}

} // namespace nnh
