#pragma once
// Shared helpers for the pathfinding tests (spec 025): misuse runs and small maps drawn in ASCII.

#include <cstring>

#include "crash/crash_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/pathfinding.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace path_test {

/// The value of a Result that must succeed (moved out).
template <typename T>
T take(nnh::Result<T> r) {
    CHECK(r.ok());
    return static_cast<T&&>(r.value());
}

/// FNV-1a 32 of four words: the seeded randomness of the tests (no rand()).
inline unsigned hash4(unsigned a, unsigned b, unsigned c, unsigned d) {
    unsigned h = 2166136261u;
    const unsigned words[4] = {a, b, c, d};
    for (unsigned w : words) {
        for (int i = 0; i < 4; ++i) {
            h ^= (w >> (8 * i)) & 0xFFu;
            h *= 16777619u;
        }
    }
    return h;
}

/// Runs `fn` and reports whether it reached the crash screen (an assert).
inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install();
    return nnh::platform_fake::run_expecting_crash(fn);
}

/// Misuse asserts in Debug/Development and is harmless in Release.
inline bool asserts_or_is_ignored(void (*fn)()) { return crashes(fn) == nnh::build::kAssertsEnabled; }

/// A map from rows of text: '#' blocked, '.' weight 1, '1'..'9' that weight, 'S'/'G'/'a'..'z' weight 1 (markers).
/// Every row has the same length.
inline nnh::PathMap ascii_map(const char* const* rows, int height,
                              nnh::Diagonals diagonals = nnh::Diagonals::OnlyIfNoObstacles, int agent = 1) {
    const int width = static_cast<int>(std::strlen(rows[0]));
    nnh::PathMapDesc d;
    d.width = width;
    d.height = height;
    d.diagonals = diagonals;
    d.agent_size = agent;
    nnh::PathMap map = take(nnh::PathMap::create(d));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const char c = rows[y][x];
            if (c == '#') {
                map.set_cell(x, y, nnh::kPathBlocked);
            } else if (c >= '1' && c <= '9') {
                map.set_cell(x, y, c - '0');
            }
        }
    }
    return map;
}

/// The first cell marked `mark` in the rows ({-1, -1} if none).
inline nnh::CellPos find_mark(const char* const* rows, int height, char mark) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; rows[y][x] != 0; ++x) {
            if (rows[y][x] == mark) return {x, y};
        }
    }
    return {-1, -1};
}

} // namespace path_test
