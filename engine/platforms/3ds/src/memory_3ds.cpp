// Nintendo 3DS platform: memory regions (contract nnh/platform/memory.hpp) — malloc heap, linear heap
// (GPU-visible) and VRAM, through libctru's allocators.

#include <3ds.h>
#include <malloc.h>

#include "nnh/platform/memory.hpp"

namespace nnh::platform {
namespace {

constexpr std::size_t kVramBytes = 6 * 1024 * 1024;

} // namespace

void* region_alloc(MemRegion region, std::size_t size, std::size_t align) {
    switch (region) {
    case MemRegion::Heap: return memalign(align, size);
    case MemRegion::Linear: return linearMemAlign(size, align);
    case MemRegion::Vram: return vramMemAlign(size, align);
    }
    return nullptr;
}

void region_free(MemRegion region, void* pointer) {
    switch (region) {
    case MemRegion::Heap: free(pointer); break;
    case MemRegion::Linear: linearFree(pointer); break;
    case MemRegion::Vram: vramFree(pointer); break;
    }
}

std::size_t region_capacity(MemRegion region) {
    switch (region) {
    case MemRegion::Heap: return envGetHeapSize();
    case MemRegion::Linear: return envGetLinearHeapSize();
    case MemRegion::Vram: return kVramBytes;
    }
    return 0;
}

} // namespace nnh::platform
