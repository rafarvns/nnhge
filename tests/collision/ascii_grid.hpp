#pragma once
// Test helper: a collision grid drawn as text. '.' empty, '#' solid, '=' one-way. Rows must have equal
// length.

#include <cstring>

#include "nnh/collision.hpp"

namespace nnh::test {

struct AsciiGrid {
    static constexpr int kMaxCells = 2048;
    collision::Cell cells[kMaxCells] = {};
    collision::Grid grid;

    AsciiGrid(const char* const* rows, int height, float cell_w = 16, float cell_h = 16, Vec2 origin = {}) {
        const int width = height > 0 ? static_cast<int>(std::strlen(rows[0])) : 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) cells[y * width + x] = kind(rows[y][x]);
        }
        grid = collision::grid_view(cells, width, height, cell_w, cell_h, origin);
    }

    static collision::Cell kind(char c) {
        if (c == '#') return collision::Cell::Solid;
        if (c == '=') return collision::Cell::OneWay;
        return collision::Cell::Empty;
    }
};

inline bool same_rect(Rect a, Rect b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }

inline bool no_sides(collision::Sides s) { return !s.left && !s.right && !s.ceiling && !s.floor; }

} // namespace nnh::test
