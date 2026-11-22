#pragma once
// The tweens example's scenes and how SELECT moves between them: showcase -> self-check -> bench -> showcase.

#include <nnh/log.hpp>
#include <nnh/scene.hpp>

NNH_LOG_CHANNEL_EXTERN(tweens);

struct Demos {
    nnh::Scene* showcase = nullptr;
    nnh::Scene* overlay = nullptr; ///< pushed over the showcase (X), popped with START
    nnh::Scene* check = nullptr;
    nnh::Scene* bench = nullptr;
};
