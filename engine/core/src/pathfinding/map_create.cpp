// Path maps (spec 025 FR-1, FR-2, FR-6): desc validation (sizes, costs, the 32-bit bound), one block of tagged
// memory (header, padded cells, source bytes), moves, and the doomed block a destroyed map leaves to its users.

#include <new>

#include "nnh/assert.hpp"
#include "nnh/memory.hpp"
#include "pathfinding/path_block.hpp"
#include "pathfinding/path_check.hpp"

NNH_MEM_TAG(path);

namespace nnh {
namespace {

bool finite_positive(float v) { return v == v && v > 0 && v - v == 0; }

/// The desc's error, or ErrorCode::None.
Error check(const PathMapDesc& d) {
    const int k = detail::kMaxPathSide;
    if (d.width < 1 || d.width > k || d.height < 1 || d.height > k) {
        return Error::make(ErrorCode::InvalidArgument, "path map %d x %d: sides must be 1..%d", d.width, d.height, k);
    }
    if (!finite_positive(d.cell_w) || !finite_positive(d.cell_h)) {
        return Error::make(ErrorCode::InvalidArgument, "path map: cell size must be finite and > 0");
    }
    if (d.straight_cost < 1 || d.diagonal_cost < d.straight_cost || d.diagonal_cost > 2 * d.straight_cost) {
        return Error::make(ErrorCode::InvalidArgument, "path map: costs %d/%d (need 1 <= straight <= diagonal <= 2x)",
                           d.straight_cost, d.diagonal_cost);
    }
    if (d.agent_size < 1 || d.agent_size > 4) {
        return Error::make(ErrorCode::InvalidArgument, "path map: agent size %d not 1..4", d.agent_size);
    }
    // FR-6: the worst path cost (cells x diagonal x 255) plus the largest scaled heuristic fit in 32 bits.
    const unsigned long long worst = static_cast<unsigned long long>(d.width) * static_cast<unsigned long long>(d.height) *
                                     static_cast<unsigned long long>(d.diagonal_cost) * 259ull;
    if (worst >= (1ull << 32)) {
        return Error::make(ErrorCode::InvalidArgument, "path map %d x %d: diagonal cost %d overflows 32 bits",
                           d.width, d.height, d.diagonal_cost);
    }
    return Error{ErrorCode::None, {}};
}

std::size_t block_bytes(int w, int h, int agent) {
    const std::size_t padded = static_cast<std::size_t>(w + 2) * static_cast<std::size_t>(h + 2);
    const std::size_t source = agent > 1 ? static_cast<std::size_t>(w) * static_cast<std::size_t>(h) : 0;
    return sizeof(detail::PathMapBlock) + padded + source;
}

void free_block(detail::PathMapBlock* b) {
    b->~PathMapBlock();
    mem::free(b);
}

} // namespace

namespace detail {

MemTag& path_memory_tag() { return NNH_MEM_TAG_REF(path); }

void path_map_release(PathMapBlock* block) {
    if (block == nullptr) return;
    --block->users;
    if (block->users <= 0 && block->doomed) free_block(block);
}

} // namespace detail

Result<PathMap> PathMap::create(const PathMapDesc& desc) {
    const Error error = check(desc);
    if (error.code != ErrorCode::None) return error;
    const std::size_t bytes = block_bytes(desc.width, desc.height, desc.agent_size);
    MemTag& tag = desc.tag != nullptr ? *desc.tag : detail::path_memory_tag();
    auto* raw = static_cast<unsigned char*>(mem::alloc(MemRegion::Heap, tag, bytes));
    if (raw == nullptr) {
        return Error::make(ErrorCode::OutOfMemory, "path map %d x %d: no memory for %zu bytes", desc.width,
                           desc.height, bytes);
    }
    auto* b = ::new (raw) detail::PathMapBlock{};
    b->width = desc.width;
    b->height = desc.height;
    b->stride = desc.width + 2;
    b->grid_x = desc.grid_x;
    b->grid_y = desc.grid_y;
    b->origin = desc.origin;
    b->cell_w = desc.cell_w;
    b->cell_h = desc.cell_h;
    b->straight = desc.straight_cost;
    b->diagonal = desc.diagonal_cost;
    b->agent = desc.agent_size;
    b->diagonals = desc.diagonals;
    b->one_way_walkable = desc.one_way_walkable;
    b->cells = raw + sizeof(detail::PathMapBlock);
    const int padded = b->stride * (desc.height + 2);
    for (int i = 0; i < padded; ++i) b->cells[i] = 0;
    for (int y = 0; y < desc.height; ++y) {
        std::uint8_t* row = b->cells + detail::padded_index(b->stride, 0, y);
        for (int x = 0; x < desc.width; ++x) row[x] = 1;
    }
    if (desc.agent_size > 1) {
        b->source = b->cells + padded;
        for (int i = 0; i < desc.width * desc.height; ++i) b->source[i] = 1;
        // The footprint: the last k - 1 columns and rows can't hold the whole agent.
        const int k = desc.agent_size;
        for (int y = 0; y < desc.height; ++y) {
            for (int x = 0; x < desc.width; ++x) {
                if (x + k > desc.width || y + k > desc.height) b->cells[detail::padded_index(b->stride, x, y)] = 0;
            }
        }
    }
    PathMap map;
    map.block_ = b;
    return map;
}

PathMap::PathMap(PathMap&& other) noexcept : block_(other.block_) { other.block_ = nullptr; }

PathMap& PathMap::operator=(PathMap&& other) noexcept {
    if (this != &other) {
        PathMap gone(static_cast<PathMap&&>(*this));
        block_ = other.block_;
        other.block_ = nullptr;
    }
    return *this;
}

PathMap::~PathMap() {
    detail::PathMapBlock* b = block_;
    if (b == nullptr) return;
    block_ = nullptr;
    if (b->users > 0) {
        // Kept for the searches and builds still running over it: their next step ends Invalid and frees it.
        b->doomed = true;
        detail::path_misuse("PathMap destroyed while a sliced search or field build runs over it");
        return;
    }
    free_block(b);
}

bool PathMap::valid() const { return block_ != nullptr; }

PathMapDesc path_map_desc(const collision::Grid& grid, CellRect region) {
    PathMapDesc d;
    d.origin = grid.origin;
    d.cell_w = grid.cell_w;
    d.cell_h = grid.cell_h;
    d.grid_x = region.x;
    d.grid_y = region.y;
    d.width = region.w < 0 ? grid.width - region.x : region.w;
    d.height = region.h < 0 ? grid.height - region.y : region.h;
    return d;
}

} // namespace nnh
