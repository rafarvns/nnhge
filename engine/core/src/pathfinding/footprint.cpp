// Agent footprints (spec 025 FR-5), see footprint.hpp.

#include "pathfinding/footprint.hpp"

namespace nnh::detail {
namespace {

/// The searchable byte of anchor (x, y): 0 if the square leaves the map or holds a blocked cell, else its max.
std::uint8_t eroded(const PathMapBlock& b, int x, int y) {
    const int k = b.agent;
    if (x + k > b.width || y + k > b.height) return 0;
    std::uint8_t worst = 0;
    for (int dy = 0; dy < k; ++dy) {
        const std::uint8_t* row = b.source + (y + dy) * b.width + x;
        for (int dx = 0; dx < k; ++dx) {
            const std::uint8_t v = row[dx];
            if (v == 0) return 0;
            if (v > worst) worst = v;
        }
    }
    return worst;
}

} // namespace

void footprint_refresh(PathMapBlock& b, int x0, int y0, int x1, int y1) {
    if (b.source == nullptr) return;
    const int k = b.agent;
    const int ax0 = x0 - (k - 1) < 0 ? 0 : x0 - (k - 1);
    const int ay0 = y0 - (k - 1) < 0 ? 0 : y0 - (k - 1);
    for (int y = ay0; y <= y1; ++y) {
        std::uint8_t* row = b.cells + padded_index(b.stride, 0, y);
        for (int x = ax0; x <= x1; ++x) row[x] = eroded(b, x, y);
    }
}

} // namespace nnh::detail
