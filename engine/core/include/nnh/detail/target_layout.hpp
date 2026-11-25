// Internal: what 3DS-only size checks test for. Not for games.
#pragma once

#include <cstddef>

namespace nnh::detail {

/// True where types lay out as on the 3DS: 32-bit pointers and an 8-byte `std::max_align_t` (the alignment of
/// SmallFunction's storage). 32-bit x86 aligns `max_align_t` to 16 and pads more, so size budgets written for the
/// console are checked only here, never on a 32-bit PC build.
inline constexpr bool kThreeDsLayout = sizeof(void*) == 4 && alignof(std::max_align_t) == 8;

} // namespace nnh::detail
