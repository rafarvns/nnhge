#pragma once
// Log channels used by the engine itself, defined in log/engine_channels.cpp. They live in namespace
// nnh, so games can declare channels with the same names in the global namespace without clashing.

#include "nnh/log.hpp"

namespace nnh {

NNH_LOG_CHANNEL_EXTERN(app);
NNH_LOG_CHANNEL_EXTERN(assets);
NNH_LOG_CHANNEL_EXTERN(events);
NNH_LOG_CHANNEL_EXTERN(gfx);
NNH_LOG_CHANNEL_EXTERN(input);
NNH_LOG_CHANNEL_EXTERN(memory);

} // namespace nnh
