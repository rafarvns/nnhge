#pragma once
// The doors' transition (spec 023 AC-7): an iris closing on the player across both screens (the tall area), and
// reopening on the new level's spawn point (LevelScene::on_enter calls transition::reveal_at).

#include <nnh/transition.hpp>

/// Iris circle, 4-pixel bands, black, tall area; out 36 frames, hold 6, in 36, QuadIn; input blocked. `centre`: the
/// player's top-screen point.
nnh::transition::Options door_transition(nnh::Vec2 centre);
