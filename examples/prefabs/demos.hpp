#pragma once
// The prefabs example's scenes and how the buttons move between them: SELECT runs the self-check, Y the bench, and
// both come back to the showcase.

#include <nnh/log.hpp>
#include <nnh/scene.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

struct Demos {
    nnh::Scene* showcase = nullptr;
    nnh::Scene* check = nullptr;
    nnh::Scene* bench = nullptr;
};

/// PREFABS_DEMO=<name> (simulator screenshots, deterministic): is it `name`?
bool demo_is(const char* name);
