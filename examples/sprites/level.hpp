#pragma once
// The level file (assets/levels/level1.txt): data the game reads from its assets (spec 007).
//
//   # comment
//   platform <first tile> <last tile> <y>    floating platform (whole numbers); repeats every 16 tiles

#include <cstddef>

struct Platform {
    int first = 0; ///< first tile column (0..15) in each 16-tile stretch
    int last = 0;
    float y = 0;   ///< top of the grass, in world pixels
};

struct Level {
    static constexpr int kMaxPlatforms = 8;
    Platform platforms[kMaxPlatforms];
    int count = 0;
};

/// Parses the level text (`size` bytes, no terminator needed). Unknown lines are ignored.
/// @return false if a platform line is malformed or there are too many.
bool parse_level(const char* text, std::size_t size, Level& out);
