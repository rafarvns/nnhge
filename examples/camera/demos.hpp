#pragma once
// The camera example's scenes and how SELECT moves between them: play -> self-check -> bench -> play.

#include <nnh/log.hpp>
#include <nnh/scene.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

struct Demos {
    nnh::Scene* play = nullptr;
    nnh::Scene* check = nullptr;
    nnh::Scene* bench = nullptr;
};

/// CAMERA_DEMO=<name> (simulator screenshots, deterministic): is it `name`?
bool demo_is(const char* name);
