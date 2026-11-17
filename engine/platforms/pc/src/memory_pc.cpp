// PC platform: memory regions (contract nnh/platform/memory.hpp). Linear and Vram are emulated with the
// normal heap but limited to Old 3DS capacities, so a game that wouldn't fit on the console fails on PC too.

#include <cstdint>
#include <cstdlib>

#include "nnh/platform/memory.hpp"

namespace nnh::platform {
namespace {

constexpr std::size_t kMiB = 1024 * 1024;
constexpr std::size_t kCapacity[kMemRegionCount] = {0, 32 * kMiB, 6 * kMiB}; // heap unlimited on PC
std::size_t g_used[kMemRegionCount] = {};

// Each block stores its raw pointer and size just before the aligned address.
struct Prefix {
    void* raw;
    std::size_t size;
};

} // namespace

void* region_alloc(MemRegion region, std::size_t size, std::size_t align) {
    const int r = static_cast<int>(region);
    if (kCapacity[r] != 0 && g_used[r] + size > kCapacity[r]) return nullptr;
    if (align < alignof(Prefix)) align = alignof(Prefix);
    void* raw = std::malloc(size + align + sizeof(Prefix));
    if (raw == nullptr) return nullptr;
    const auto start = reinterpret_cast<std::uintptr_t>(raw) + sizeof(Prefix);
    const std::uintptr_t aligned = (start + align - 1) & ~(static_cast<std::uintptr_t>(align) - 1);
    *reinterpret_cast<Prefix*>(aligned - sizeof(Prefix)) = Prefix{raw, size};
    g_used[r] += size;
    return reinterpret_cast<void*>(aligned);
}

void region_free(MemRegion region, void* pointer) {
    if (pointer == nullptr) return;
    const Prefix prefix = *reinterpret_cast<Prefix*>(static_cast<unsigned char*>(pointer) - sizeof(Prefix));
    g_used[static_cast<int>(region)] -= prefix.size;
    std::free(prefix.raw);
}

std::size_t region_capacity(MemRegion region) { return kCapacity[static_cast<int>(region)]; }

} // namespace nnh::platform
