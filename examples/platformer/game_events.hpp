#pragma once
// The example's own events (spec 001): a fact the levels publish and a request only the demo switcher
// (demos.hpp) handles. Engine events (nnh::ButtonPressed, ...) live in the engine's headers.

#include <cstdint>

/// The example's demos, in SELECT order.
enum class Demo : std::uint8_t { Level1, Bonus, Bench, Flips };

/// A coin was picked up (published by a level, counted by its HUD).
struct CoinCollected {
    int value;
};

/// The player (re)started at the level's spawn point with every coin back (published by a level).
struct LevelRestarted {
    int coins; ///< coins in the level
};

/// Request: switch to `demo` (published by a level's door and by the replay; handled by Demos).
struct ShowDemo {
    Demo demo;
};
