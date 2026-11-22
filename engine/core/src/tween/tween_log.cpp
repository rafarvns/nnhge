// The module's log channel, alone in its object file: nnh::Timer logs misuse through it without pulling the
// tween set code into a game that only uses timers (spec 015 FR-19).

#include "nnh/log.hpp"

NNH_LOG_CHANNEL(tween);
