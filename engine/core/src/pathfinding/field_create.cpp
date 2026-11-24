// Distance fields (spec 025 FR-16): one block of tagged memory (header, the 8-byte open set, one or two u32 arrays per
// padded cell and as many step bytes), moves, destruction (a running build lets its map go).

#include <new>

#include "nnh/memory.hpp"
#include "pathfinding/field_internal.hpp"
#include "pathfinding/path_check.hpp"

namespace nnh {

static_assert(sizeof(detail::FieldKey) == 8, "a field heap entry is 8 bytes");

Result<DistanceField> DistanceField::create(const DistanceFieldDesc& desc) {
    const int k = detail::kMaxPathSide;
    if (desc.width < 1 || desc.width > k || desc.height < 1 || desc.height > k) {
        return Error::make(ErrorCode::InvalidArgument, "distance field %d x %d: sides must be 1..%d", desc.width,
                           desc.height, k);
    }
    const std::size_t records = static_cast<std::size_t>(desc.width + 2) * static_cast<std::size_t>(desc.height + 2);
    const std::size_t entries = static_cast<std::size_t>(desc.width) * static_cast<std::size_t>(desc.height);
    const std::size_t arrays = desc.double_buffered ? 2 : 1;
    // The heap starts 8 bytes before a 32-byte line: every node's four children share a line (field_heap.hpp).
    const std::size_t header = ((sizeof(detail::FieldBlock) + 31) & ~static_cast<std::size_t>(31)) + 24;
    const std::size_t heap_bytes = entries * sizeof(detail::FieldKey);
    const std::size_t bytes = header + heap_bytes + arrays * records * (4 + 1);
    auto* raw = static_cast<unsigned char*>(
        mem::alloc(MemRegion::Heap, desc.tag != nullptr ? *desc.tag : detail::path_memory_tag(), bytes, 32));
    if (raw == nullptr) {
        return Error::make(ErrorCode::OutOfMemory, "distance field %d x %d: no memory for %zu bytes", desc.width,
                           desc.height, bytes);
    }
    auto* b = ::new (raw) detail::FieldBlock{};
    b->width = desc.width;
    b->height = desc.height;
    b->records = static_cast<int>(records);
    b->double_buffered = desc.double_buffered;
    b->heap = reinterpret_cast<detail::FieldKey*>(raw + header);
    b->heap_capacity = static_cast<int>(entries);
    b->dist[0] = reinterpret_cast<std::uint32_t*>(raw + header + heap_bytes);
    b->dist[1] = desc.double_buffered ? b->dist[0] + records : b->dist[0];
    b->steps[0] = reinterpret_cast<std::uint8_t*>(b->dist[0] + arrays * records);
    b->steps[1] = desc.double_buffered ? b->steps[0] + records : b->steps[0];
    DistanceField f;
    f.block_ = b;
    return f;
}

DistanceField::DistanceField(DistanceField&& other) noexcept : block_(other.block_) { other.block_ = nullptr; }

DistanceField& DistanceField::operator=(DistanceField&& other) noexcept {
    if (this != &other) {
        DistanceField gone(static_cast<DistanceField&&>(*this));
        block_ = other.block_;
        other.block_ = nullptr;
    }
    return *this;
}

DistanceField::~DistanceField() {
    if (block_ == nullptr) return;
    detail::field_release_map(*block_);
    block_->~FieldBlock();
    mem::free(block_);
    block_ = nullptr;
}

bool DistanceField::valid() const { return block_ != nullptr; }
int DistanceField::width() const { return block_ != nullptr ? block_->width : 0; }
int DistanceField::height() const { return block_ != nullptr ? block_->height : 0; }
bool DistanceField::building() const { return block_ != nullptr && block_->building; }
bool DistanceField::ready() const { return block_ != nullptr && block_->ready; }
bool DistanceField::stale() const { return block_ != nullptr && block_->ready && block_->stale; }
int DistanceField::settled() const { return block_ != nullptr ? block_->settled : 0; }

namespace detail {

void field_release_map(FieldBlock& f) {
    if (!f.holds_map) return;
    f.holds_map = false;
    path_map_release(f.map);
    f.map = nullptr;
}

} // namespace detail
} // namespace nnh
