#pragma once
/// @file nnh/platform/memory.hpp
/// Platform contract — raw memory per region. Implemented by every platform library, called only by
/// nnh::core (nnh::mem adds tags, stats and alignment on top). Not for games.

#include <cstddef>

#include "nnh/memory.hpp"

namespace nnh::platform {

/// Allocates `size` bytes in `region`, aligned to at least `align`. nullptr when the region is full.
/// Vram memory must not be written by the CPU on real hardware; core never writes into it.
void* region_alloc(MemRegion region, std::size_t size, std::size_t align);

/// Frees memory returned by region_alloc() for the same region.
void region_free(MemRegion region, void* pointer);

/// Total bytes available in the region on this system (0 = unknown / unlimited).
std::size_t region_capacity(MemRegion region);

} // namespace nnh::platform
