#pragma once
// Memory accounting for other engine modules. Internal.

#include <cstddef>

#include "nnh/memory.hpp"

namespace nnh::detail {

/// Counts (`add`) or uncounts memory the platform allocated itself (e.g. a GPU texture created by
/// citro3d or SDL) under `tag`, so it shows in region/tag stats, budgets and the leak report like
/// memory from nnh::mem::alloc. Uncount exactly what was counted.
void mem_account(MemRegion region, MemTag& tag, std::size_t bytes, bool add);

} // namespace nnh::detail
