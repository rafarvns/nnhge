// Path map edits and queries (spec 025 FR-2, FR-4): set_cell, fill, set_diagonals, the cells searches see, the
// version counter (+1 per accepted call) and the accessors.

#include "pathfinding/footprint.hpp"
#include "pathfinding/path_block.hpp"
#include "pathfinding/path_check.hpp"

namespace nnh {
namespace {

std::uint8_t byte_of(int weight) { return weight == kPathBlocked ? 0 : static_cast<std::uint8_t>(weight); }

detail::PathMapBlock* live(detail::PathMapBlock* b, const char* who) {
    if (b == nullptr) {
        detail::path_misuse(who);
        return nullptr;
    }
    return b;
}

} // namespace

void PathMap::set_cell(int x, int y, int weight) {
    detail::PathMapBlock* b = live(block_, "PathMap::set_cell on an empty map");
    if (b == nullptr || !detail::cell_ok(b->width, b->height, x, y, "PathMap::set_cell") ||
        !detail::weight_ok(weight, "PathMap::set_cell")) {
        return;
    }
    detail::write_cell(*b, x, y, byte_of(weight));
    detail::footprint_refresh(*b, x, y, x, y);
    ++b->version;
}

void PathMap::fill(CellRect region, int weight) {
    detail::PathMapBlock* b = live(block_, "PathMap::fill on an empty map");
    detail::CellSpan s;
    if (b == nullptr || !detail::weight_ok(weight, "PathMap::fill") ||
        !detail::clip_region(region, b->width, b->height, s, "PathMap::fill")) {
        return;
    }
    const std::uint8_t v = byte_of(weight);
    for (int y = s.y0; y <= s.y1; ++y) {
        for (int x = s.x0; x <= s.x1; ++x) detail::write_cell(*b, x, y, v);
    }
    if (!s.empty()) detail::footprint_refresh(*b, s.x0, s.y0, s.x1, s.y1);
    ++b->version;
}

void PathMap::set_diagonals(Diagonals diagonals) {
    detail::PathMapBlock* b = live(block_, "PathMap::set_diagonals on an empty map");
    if (b == nullptr) return;
    if (static_cast<int>(diagonals) > static_cast<int>(Diagonals::OnlyIfNoObstacles)) {
        detail::path_misuse("PathMap::set_diagonals: not a Diagonals value");
        return;
    }
    b->diagonals = diagonals;
    ++b->version;
}

bool PathMap::blocked(int x, int y) const {
    const detail::PathMapBlock* b = block_;
    if (b == nullptr || !detail::cell_ok(b->width, b->height, x, y, "PathMap::blocked")) return true;
    return b->cells[detail::padded_index(b->stride, x, y)] == 0;
}

int PathMap::weight(int x, int y) const {
    const detail::PathMapBlock* b = block_;
    if (b == nullptr || !detail::cell_ok(b->width, b->height, x, y, "PathMap::weight")) return kPathBlocked;
    const std::uint8_t v = b->cells[detail::padded_index(b->stride, x, y)];
    return v == 0 ? kPathBlocked : v;
}

std::uint32_t PathMap::version() const { return block_ != nullptr ? block_->version : 0; }
int PathMap::width() const { return block_ != nullptr ? block_->width : 0; }
int PathMap::height() const { return block_ != nullptr ? block_->height : 0; }
int PathMap::agent_size() const { return block_ != nullptr ? block_->agent : 0; }
Diagonals PathMap::diagonals() const { return block_ != nullptr ? block_->diagonals : Diagonals::Never; }
int PathMap::straight_cost() const { return block_ != nullptr ? block_->straight : 0; }
int PathMap::diagonal_cost() const { return block_ != nullptr ? block_->diagonal : 0; }

bool PathMap::contains(CellPos c) const {
    return block_ != nullptr && c.x >= 0 && c.y >= 0 && c.x < block_->width && c.y < block_->height;
}

} // namespace nnh
