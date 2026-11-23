#pragma once
// Pools (spec 020): the module's internals shared by its source files.

#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "nnh/pool.hpp"

NNH_LOG_CHANNEL_EXTERN(pool);

namespace nnh::detail {

/// The engine's default tag for run-time pools (`pool`).
MemTag& pool_memory_tag();

} // namespace nnh::detail
