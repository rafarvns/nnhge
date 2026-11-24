#pragma once
// The particles example's scenes and how they reach each other: showcase -> SELECT self-check -> showcase,
// showcase -> Y bench -> SELECT showcase.

#include <nnh/log.hpp>
#include <nnh/scene.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

struct Demos {
    nnh::Scene* showcase = nullptr;
    nnh::Scene* check = nullptr;
    nnh::Scene* bench = nullptr;
};

/// PARTICLES_DEMO=<name>: is it `name`? (showcase (default) | check | bench)
bool demo_is(const char* name);
/// PARTICLES_SHOT (simulator screenshots): gallery | trail | explosion | touch | local | world | full, or "" when unset.
const char* shot_name();
