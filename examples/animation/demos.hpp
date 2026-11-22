#pragma once
// The animation example's scenes and how SELECT moves between them: showcase -> self-check -> bench -> showcase.

#include <nnh/log.hpp>
#include <nnh/scene.hpp>

NNH_LOG_CHANNEL_EXTERN(anim);

struct Demos {
    nnh::Scene* showcase = nullptr;
    nnh::Scene* check = nullptr;
    nnh::Scene* bench = nullptr;
};
