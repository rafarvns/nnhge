// Agent footprints (spec 025 FR-5; T1.4): sizes 1-4 against a brute-force erosion on seeded maps, the square's largest
// weight, the last k - 1 columns and rows blocked, partial syncs and set_cell next to a changed cell.

#include <vector>

#include "nnh/collision.hpp"
#include "path_fixture.hpp"

using nnh::kPathBlocked;
using nnh::PathMap;

namespace {

/// Brute force: the value a k-agent map shows at (x, y) for the source values `src` (0 blocked).
int eroded(const std::vector<int>& src, int w, int h, int k, int x, int y) {
    if (x + k > w || y + k > h) return kPathBlocked;
    int worst = 0;
    for (int dy = 0; dy < k; ++dy) {
        for (int dx = 0; dx < k; ++dx) {
            const int v = src[(y + dy) * w + x + dx];
            if (v == 0) return kPathBlocked;
            if (v > worst) worst = v;
        }
    }
    return worst;
}

PathMap make(int w, int h, int k) {
    nnh::PathMapDesc d;
    d.width = w;
    d.height = h;
    d.agent_size = k;
    return path_test::take(PathMap::create(d));
}

bool matches(const PathMap& map, const std::vector<int>& src, int w, int h, int k) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (map.weight(x, y) != eroded(src, w, h, k, x, y)) return false;
        }
    }
    return true;
}

} // namespace

NNH_TEST(path_footprint_equals_brute_force_erosion_on_200_seeded_maps) {
    for (unsigned seed = 0; seed < 200; ++seed) {
        const int w = 1 + static_cast<int>(path_test::hash4(seed, 1, 0, 0) % 12);
        const int h = 1 + static_cast<int>(path_test::hash4(seed, 2, 0, 0) % 12);
        const int k = 1 + static_cast<int>(seed % 4);
        std::vector<int> src(static_cast<std::size_t>(w * h));
        PathMap map = make(w, h, k);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const unsigned r = path_test::hash4(seed, static_cast<unsigned>(x), static_cast<unsigned>(y), 3);
                const int v = r % 5 == 0 ? 0 : 1 + static_cast<int>((r >> 8) % 9);
                src[static_cast<std::size_t>(y * w + x)] = v;
                map.set_cell(x, y, v == 0 ? kPathBlocked : v);
            }
        }
        CHECK(matches(map, src, w, h, k));
    }
}

NNH_TEST(path_footprint_blocks_the_last_columns_and_rows_and_takes_the_heaviest_cell) {
    PathMap map = make(5, 4, 3);
    map.set_cell(2, 2, 6);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 5; ++x) {
            if (x > 2 || y > 1) CHECK(map.blocked(x, y));
        }
    }
    CHECK(map.weight(0, 0) == 6 && map.weight(1, 1) == 6 && map.weight(2, 0) == 6);
    map.set_cell(2, 2, 1);
    CHECK(map.weight(0, 0) == 1);
    map.set_cell(0, 0, kPathBlocked); // only anchor (0, 0)'s square holds (0, 0)
    CHECK(map.blocked(0, 0) && !map.blocked(1, 0) && !map.blocked(0, 1) && !map.blocked(1, 1));
}

NNH_TEST(path_footprint_follows_partial_syncs_and_fills_exactly) {
    using nnh::collision::Cell;
    for (int k = 1; k <= 4; ++k) {
        const int w = 9;
        const int h = 8;
        std::vector<Cell> cells(static_cast<std::size_t>(w * h), Cell::Empty);
        std::vector<int> src(static_cast<std::size_t>(w * h), 1);
        PathMap map = make(w, h, k);
        const nnh::collision::Grid g = nnh::collision::grid_view(cells.data(), w, h, 16, 16);
        map.sync(g);
        CHECK(matches(map, src, w, h, k));
        // A door: three cells become solid, the sync covers only them.
        for (int x = 4; x <= 6; ++x) {
            cells[static_cast<std::size_t>(5 * w + x)] = Cell::Solid;
            src[static_cast<std::size_t>(5 * w + x)] = 0;
        }
        map.sync(g, {4, 5, 3, 1});
        CHECK(matches(map, src, w, h, k));
        // A swamp next to it.
        map.fill({2, 4, 2, 2}, 7);
        for (int y = 4; y < 6; ++y) {
            for (int x = 2; x < 4; ++x) src[static_cast<std::size_t>(y * w + x)] = 7;
        }
        CHECK(matches(map, src, w, h, k));
        // The door opens.
        for (int x = 4; x <= 6; ++x) {
            cells[static_cast<std::size_t>(5 * w + x)] = Cell::Empty;
            src[static_cast<std::size_t>(5 * w + x)] = 1;
        }
        map.sync(g, {4, 5, 3, 1});
        CHECK(matches(map, src, w, h, k));
    }
}
