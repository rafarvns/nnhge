// Searchers (spec 025 FR-7): one block of tagged memory (header, 8-byte records per padded cell, the 16-byte open
// set), moves, destruction (a running search lets its map go), the stamp test hook.

#include <new>

#include "nnh/memory.hpp"
#include "pathfinding/path_check.hpp"
#include "pathfinding/search_internal.hpp"

namespace nnh {

static_assert(sizeof(detail::SearchRecord) == 8, "a search record is 8 bytes");
static_assert(sizeof(detail::OpenEntry) == 16, "an open entry is 16 bytes");

Result<PathSearcher> PathSearcher::create(int width, int height, MemTag* tag) {
    const int k = detail::kMaxPathSide;
    if (width < 1 || width > k || height < 1 || height > k) {
        return Error::make(ErrorCode::InvalidArgument, "path searcher %d x %d: sides must be 1..%d", width, height, k);
    }
    const std::size_t records = static_cast<std::size_t>(width + 2) * static_cast<std::size_t>(height + 2);
    const std::size_t entries = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::size_t header = (sizeof(detail::PathSearchBlock) + 15) & ~static_cast<std::size_t>(15);
    const std::size_t bytes = header + records * sizeof(detail::SearchRecord) + entries * sizeof(detail::OpenEntry);
    auto* raw = static_cast<unsigned char*>(
        mem::alloc(MemRegion::Heap, tag != nullptr ? *tag : detail::path_memory_tag(), bytes, 16));
    if (raw == nullptr) {
        return Error::make(ErrorCode::OutOfMemory, "path searcher %d x %d: no memory for %zu bytes", width, height,
                           bytes);
    }
    auto* b = ::new (raw) detail::PathSearchBlock{};
    b->width = width;
    b->height = height;
    b->records = static_cast<int>(records);
    b->record = reinterpret_cast<detail::SearchRecord*>(raw + header);
    for (std::size_t i = 0; i < records; ++i) b->record[i] = {};
    b->heap.data = reinterpret_cast<detail::OpenEntry*>(raw + header + records * sizeof(detail::SearchRecord));
    b->heap.capacity = static_cast<int>(entries);
    PathSearcher s;
    s.block_ = b;
    return s;
}

PathSearcher::PathSearcher(PathSearcher&& other) noexcept : block_(other.block_) { other.block_ = nullptr; }

PathSearcher& PathSearcher::operator=(PathSearcher&& other) noexcept {
    if (this != &other) {
        PathSearcher gone(static_cast<PathSearcher&&>(*this));
        block_ = other.block_;
        other.block_ = nullptr;
    }
    return *this;
}

PathSearcher::~PathSearcher() {
    if (block_ == nullptr) return;
    detail::search_release_map(*block_);
    block_->~PathSearchBlock();
    mem::free(block_);
    block_ = nullptr;
}

bool PathSearcher::valid() const { return block_ != nullptr; }
int PathSearcher::width() const { return block_ != nullptr ? block_->width : 0; }
int PathSearcher::height() const { return block_ != nullptr ? block_->height : 0; }

namespace detail {

void path_searcher_set_stamp(PathSearcher& searcher, std::uint32_t stamp) {
    PathSearchBlock* b = PathAccess::searcher(searcher);
    if (b != nullptr) b->stamp = stamp;
}

void search_release_map(PathSearchBlock& s) {
    if (!s.holds_map) return;
    s.holds_map = false;
    path_map_release(s.map);
    s.map = nullptr; // a doomed map may be gone now
}

} // namespace detail
} // namespace nnh
