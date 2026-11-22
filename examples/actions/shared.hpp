#pragma once
// What the actions example's scenes share: the one action set (so remaps apply everywhere), the self-check and the
// scenes they switch to. Owned by main().

#include "controls.hpp"
#include "self_check.hpp"

#include <nnh/log.hpp>
#include <nnh/scene.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

struct Shared {
    nnh::ActionSet controls{{.actions = static_cast<int>(Act::Count), .contexts = static_cast<int>(Ctx::Count)}};
    ActionsSelfCheck check;
    nnh::Scene* controls_scene = nullptr;
    nnh::Scene* bench_scene = nullptr;
};

/// Logs one fixed-format line (the console script expects them).
void log_line(const char* line);
