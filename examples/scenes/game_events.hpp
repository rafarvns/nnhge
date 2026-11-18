#pragma once
// The example's own events. Engine events (nnh::ButtonPressed) live in <nnh/input.hpp>.

/// A coin was picked up (published by the level, counted by Score).
struct CoinCollected {
    int value;
};
