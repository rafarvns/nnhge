#pragma once
/// @file nnh/memory.hpp
/// Tagged allocations per memory region, with usage stats, budgets and a leak report.
/// See docs/api/memory.md and docs/architecture/foundation.md#memory.
///
/// @code
/// NNH_MEM_TAG(level);                                    // once, in one .cpp
/// void* tiles = nnh::mem::alloc(nnh::MemRegion::Heap, NNH_MEM_TAG_REF(level), 64 * 1024);
/// ...
/// nnh::mem::free(tiles);
/// @endcode
/// Allocate at load time, not every frame.

#include <cstddef>
#include <cstdint>

namespace nnh {

/// Where memory comes from. On 3DS: Heap = malloc heap, Linear = GPU-visible heap (textures, vertex and
/// audio buffers), Vram = 6 MB of video memory the CPU can't write. The PC simulator emulates
/// Linear and Vram with Old 3DS capacities so overruns show up on PC first.
enum class MemRegion : std::uint8_t { Heap, Linear, Vram };
inline constexpr int kMemRegionCount = 3;

/// A label for allocations ("gfx", "level", "audio"). Declare with NNH_MEM_TAG.
struct MemTag {
    const char* name;
    int slot = -1; ///< filled on first use
};

struct MemStats {
    std::size_t bytes = 0; ///< currently allocated
    std::size_t peak = 0;  ///< highest `bytes` seen
    std::size_t count = 0; ///< live allocations
};

namespace mem {

inline constexpr int kMaxTags = 32;
inline constexpr int kMaxVramAllocations = 512;

/// Allocates `size` bytes aligned to `align` (power of two) in `region`, counted under `tag`.
/// @return nullptr (and a log warning) if the system has no memory left. Going over the region's
/// budget still succeeds but logs a warning once.
void* alloc(MemRegion region, MemTag& tag, std::size_t size, std::size_t align = 8);

/// Frees memory from alloc(). nullptr is ignored. Freeing anything else is a bug (NNH_VERIFY).
void free(void* pointer);

MemStats region_stats(MemRegion region);
MemStats tag_stats(const MemTag& tag);

/// Total bytes the system offers in a region (Old 3DS values on PC). 0 if unknown.
std::size_t region_capacity(MemRegion region);

/// Warn when a region goes over `bytes` (default: its capacity). 0 disables the warning.
void set_budget(MemRegion region, std::size_t bytes);

int live_allocations();

/// Logs every tag that still owns memory (called automatically at shutdown in Debug/Development).
void report_leaks();

/// Forgets stats, tags and budgets. For tests; only valid with no live allocations.
void reset();

/// Name of a region ("heap", "linear", "vram").
const char* region_name(MemRegion region);

} // namespace mem
} // namespace nnh

/// Defines a memory tag (once per program, in a .cpp).
#define NNH_MEM_TAG(name) ::nnh::MemTag nnh_mem_tag_##name{#name}
/// Declares a tag defined in another file.
#define NNH_MEM_TAG_EXTERN(name) extern ::nnh::MemTag nnh_mem_tag_##name
/// The tag object to pass to nnh::mem::alloc.
#define NNH_MEM_TAG_REF(name) nnh_mem_tag_##name
