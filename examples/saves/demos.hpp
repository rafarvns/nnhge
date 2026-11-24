#pragma once
// The saves example's scenes and how the buttons move between them: SELECT runs the check, Y the bench, and
// both come back to the showcase with SELECT.

#include <nnh/log.hpp>
#include <nnh/scene.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

struct Demos {
    nnh::Scene* showcase = nullptr;
    nnh::Scene* check = nullptr;
    nnh::Scene* bench = nullptr;
};

/// SAVES_DEMO=<name>: showcase (default), check, bench.
bool demo_is(const char* name);

/// SAVES_SHOT=<name> (simulator screenshots: fixed actions at fixed frames): empty, saved, golden, backup.
bool shot_is(const char* name);
