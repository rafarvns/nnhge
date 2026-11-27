// Fake platform: memory regions with configurable capacities (contract nnh/platform/memory.hpp).
// Vram blocks are filled with a pattern; freeing checks it, so tests prove core never writes into VRAM. Other blocks
// may be filled with a chosen byte (set_alloc_fill) to prove core initialises what it reads.

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "fake_internal.hpp"
#include "nnh/platform/memory.hpp"

namespace nnh {
namespace {

constexpr unsigned char kVramPattern = 0xCD;

struct Prefix {
    void* raw;
    std::size_t size;
};

bool untouched(const unsigned char* p, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        if (p[i] != kVramPattern) return false;
    }
    return true;
}

} // namespace

void* platform::region_alloc(MemRegion region, std::size_t size, std::size_t align) {
    platform_fake::detail::Controls& c = platform_fake::detail::controls();
    const int r = static_cast<int>(region);
    if (c.region_used[r] + size > c.region_capacity[r]) return nullptr;
    if (align < alignof(Prefix)) align = alignof(Prefix);
    void* raw = std::malloc(size + align + sizeof(Prefix));
    if (raw == nullptr) return nullptr;
    const auto start = reinterpret_cast<std::uintptr_t>(raw) + sizeof(Prefix);
    const std::uintptr_t aligned = (start + align - 1) & ~(static_cast<std::uintptr_t>(align) - 1);
    *reinterpret_cast<Prefix*>(aligned - sizeof(Prefix)) = Prefix{raw, size};
    c.region_used[r] += size;
    if (region == MemRegion::Vram) {
        std::memset(reinterpret_cast<void*>(aligned), kVramPattern, size);
    } else if (c.alloc_fill >= 0) {
        std::memset(reinterpret_cast<void*>(aligned), c.alloc_fill, size);
    }
    return reinterpret_cast<void*>(aligned);
}

void platform::region_free(MemRegion region, void* pointer) {
    if (pointer == nullptr) return;
    const Prefix prefix = *reinterpret_cast<Prefix*>(static_cast<unsigned char*>(pointer) - sizeof(Prefix));
    if (region == MemRegion::Vram && !untouched(static_cast<unsigned char*>(pointer), prefix.size)) {
        ++platform_fake::detail::mutable_state().vram_writes_by_core;
    }
    platform_fake::detail::controls().region_used[static_cast<int>(region)] -= prefix.size;
    std::free(prefix.raw);
}

std::size_t platform::region_capacity(MemRegion region) {
    return platform_fake::detail::controls().region_capacity[static_cast<int>(region)];
}

void platform_fake::set_region_capacity(MemRegion region, std::size_t bytes) {
    detail::controls().region_capacity[static_cast<int>(region)] = bytes;
}

std::size_t platform_fake::region_used(MemRegion region) {
    return detail::controls().region_used[static_cast<int>(region)];
}

void platform_fake::set_alloc_fill(int byte) { detail::controls().alloc_fill = byte < 0 ? -1 : (byte & 0xFF); }

} // namespace nnh
