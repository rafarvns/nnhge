// Run-time pools (spec 020 FR-3): one block of tagged memory holding the core, the meta words, the three bitmaps
// and capacity + 1 objects, each part aligned. Creation and destruction only.

#include "nnh/assert.hpp"
#include "pool/pool_internal.hpp"

#include <new>

NNH_MEM_TAG(pool);

namespace nnh::detail {
namespace {

std::size_t align_up(std::size_t value, std::size_t align) { return (value + align - 1) / align * align; }

struct BlockLayout {
    std::size_t meta, bitmaps, data, total, align;
};

BlockLayout layout(int capacity, std::size_t size, std::size_t align) {
    const std::size_t words = static_cast<std::size_t>(words_for(capacity));
    BlockLayout l{};
    l.align = align > alignof(PoolCore) ? align : alignof(PoolCore);
    l.meta = align_up(sizeof(PoolCore), alignof(std::uint32_t));
    l.bitmaps = l.meta + 4 * static_cast<std::size_t>(capacity);
    l.data = align_up(l.bitmaps + 3 * 4 * words, align);
    l.total = l.data + (static_cast<std::size_t>(capacity) + 1) * size;
    return l;
}

} // namespace

MemTag& pool_memory_tag() { return NNH_MEM_TAG_REF(pool); }

std::size_t pool_block_bytes(int capacity, std::size_t size, std::size_t align) {
    return layout(capacity, size, align).total;
}

PoolCore* pool_create_block(int capacity, std::size_t size, std::size_t align, MemTag* tag, Error& error) {
    if (capacity < 1 || capacity > kMaxPoolCapacity) {
        error = Error::make(ErrorCode::InvalidArgument, "pool capacity %d outside 1..%d", capacity, kMaxPoolCapacity);
        return nullptr;
    }
    const BlockLayout l = layout(capacity, size, align);
    auto* bytes = static_cast<unsigned char*>(
        mem::alloc(MemRegion::Heap, tag != nullptr ? *tag : pool_memory_tag(), l.total, l.align < 8 ? 8 : l.align));
    if (bytes == nullptr) {
        error = Error::make(ErrorCode::OutOfMemory, "pool of %d: no memory for %zu bytes", capacity, l.total);
        return nullptr;
    }
    auto* core = ::new (bytes) PoolCore{};
    pool_init_inline(*core, capacity, reinterpret_cast<std::uint32_t*>(bytes + l.meta),
                     reinterpret_cast<std::uint32_t*>(bytes + l.bitmaps), bytes + l.data);
    return core;
}

void pool_free_block(PoolCore* core) {
    if (core == nullptr) return;
    NNH_ASSERT(core->iterating == 0, "nnh::Pool destroyed inside one of its own iterations");
    core->~PoolCore();
    mem::free(core);
}

bool pool_move_allowed(const PoolCore* core) {
    if (core == nullptr || core->scene == nullptr) return true;
    pool_misuse("moving a bound nnh::Pool (nothing moves): keep it in its scene");
    return false;
}

} // namespace nnh::detail
