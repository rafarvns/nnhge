// Seeded maps for the pathfinding self-check and bench (spec 025 AC-3, AC-7), see check_maps.hpp.

#include "check_maps.hpp"

namespace pathfinding_demo {
namespace {

std::uint32_t u(int v) { return static_cast<std::uint32_t>(v); }

std::uint8_t room_weight(std::uint32_t h) {
    const std::uint32_t r = h % 10;
    return r < 7 ? 1 : (r < 9 ? 3 : 8);
}

void open_cell(std::uint8_t* cells, int width, int x, int y, std::uint32_t seed) {
    std::uint8_t& c = cells[y * width + x];
    if (c == 0) c = room_weight(hash4(seed, u(x), u(y), 2));
}

/// A room's centre in block (bx, by), for the corridors.
nnh::CellPos room_centre(std::uint32_t seed, int bx, int by, int* rw = nullptr, int* rh = nullptr) {
    const std::uint32_t h = hash4(seed, u(bx), u(by), 1);
    const int w = 6 + static_cast<int>(h % 2);
    const int hh = 6 + static_cast<int>((h >> 1) % 2);
    const int x = bx * 8 + 1 + static_cast<int>((h >> 2) % u(8 - w));
    const int y = by * 8 + 1 + static_cast<int>((h >> 4) % u(8 - hh));
    if (rw != nullptr) *rw = w;
    if (rh != nullptr) *rh = hh;
    return {x, y}; // the top-left; the centre is (x + w / 2, y + h / 2)
}

nnh::CellPos centre_of(std::uint32_t seed, int bx, int by) {
    int w = 0, h = 0;
    const nnh::CellPos tl = room_centre(seed, bx, by, &w, &h);
    return {tl.x + w / 2, tl.y + h / 2};
}

void corridor(std::uint8_t* cells, int width, nnh::CellPos a, nnh::CellPos b, std::uint32_t seed) {
    const int sx = b.x > a.x ? 1 : -1;
    for (int x = a.x; x != b.x; x += sx) open_cell(cells, width, x, a.y, seed);
    const int sy = b.y > a.y ? 1 : -1;
    for (int y = a.y; y != b.y + sy; y += sy) open_cell(cells, width, b.x, y, seed);
}

nnh::collision::Cell byte_cell(const void* context, int x, int y) {
    const auto* g = static_cast<const ByteGrid*>(context);
    return g->cells[y * g->width + x] == 0 ? nnh::collision::Cell::Solid : nnh::collision::Cell::Empty;
}

int byte_weight(const void* context, int x, int y) {
    const auto* g = static_cast<const ByteGrid*>(context);
    return g->cells[y * g->width + x];
}

} // namespace

std::uint32_t hash4(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    std::uint32_t h = 2166136261u;
    const std::uint32_t words[4] = {a, b, c, d};
    for (std::uint32_t w : words) {
        for (int i = 0; i < 4; ++i) {
            h ^= (w >> (8 * i)) & 0xFFu;
            h *= 16777619u;
        }
    }
    return h;
}

void rooms(int width, int height, std::uint32_t seed, std::uint8_t* cells) {
    for (int i = 0; i < width * height; ++i) cells[i] = 0;
    const int bw = width / 8;
    const int bh = height / 8;
    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            int w = 0, h = 0;
            const nnh::CellPos tl = room_centre(seed, bx, by, &w, &h);
            for (int y = tl.y; y < tl.y + h; ++y) {
                for (int x = tl.x; x < tl.x + w; ++x) open_cell(cells, width, x, y, seed);
            }
        }
    }
    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            if (bx + 1 < bw) corridor(cells, width, centre_of(seed, bx, by), centre_of(seed, bx + 1, by), seed);
            if (by + 1 < bh) corridor(cells, width, centre_of(seed, bx, by), centre_of(seed, bx, by + 1), seed);
        }
    }
}

void maze(int width, int height, std::uint32_t seed, std::uint8_t* cells, int* scratch) {
    for (int i = 0; i < width * height; ++i) cells[i] = 0;
    const int cw = (width - 1) / 2; // odd cells per row and column
    const int ch = (height - 1) / 2;
    if (cw < 1 || ch < 1) return;
    static const int kDx[4] = {0, 1, 0, -1};
    static const int kDy[4] = {-1, 0, 1, 0};
    int top = 0;
    scratch[top++] = 0;
    cells[1 * width + 1] = 1;
    std::uint32_t steps = 0;
    while (top > 0) {
        const int c = scratch[top - 1];
        const int cx = c % cw;
        const int cy = c / cw;
        int options[4];
        int n = 0;
        for (int d = 0; d < 4; ++d) {
            const int nx = cx + kDx[d];
            const int ny = cy + kDy[d];
            if (nx >= 0 && ny >= 0 && nx < cw && ny < ch && cells[(2 * ny + 1) * width + 2 * nx + 1] == 0) {
                options[n++] = d;
            }
        }
        if (n == 0) {
            --top;
            continue;
        }
        const int d = options[hash4(seed, u(c), steps++, 3) % u(n)];
        const int nx = cx + kDx[d];
        const int ny = cy + kDy[d];
        cells[(2 * cy + 1 + kDy[d]) * width + 2 * cx + 1 + kDx[d]] = 1; // the wall between
        cells[(2 * ny + 1) * width + 2 * nx + 1] = 1;
        scratch[top++] = ny * cw + nx;
    }
}

void swamp(int width, int height, std::uint32_t seed, std::uint8_t* cells) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            cells[y * width + x] = static_cast<std::uint8_t>(1 + hash4(seed, u(x / 4), u(y / 4), 4) % 9);
        }
    }
    // The island: a 6 x 6 ring of walls around the middle, its 4 x 4 inside cut off.
    const int x0 = width / 2 - 3;
    const int y0 = height / 2 - 3;
    for (int i = 0; i < 6; ++i) {
        cells[y0 * width + x0 + i] = 0;
        cells[(y0 + 5) * width + x0 + i] = 0;
        cells[(y0 + i) * width + x0] = 0;
        cells[(y0 + i) * width + x0 + 5] = 0;
    }
}

nnh::collision::Grid ByteGrid::grid() const {
    nnh::collision::Grid g;
    g.width = width;
    g.height = height;
    g.outside = nnh::collision::Cell::Solid;
    g.cell = byte_cell;
    g.context = this;
    return g;
}

nnh::PathWeights ByteGrid::weights() const { return {byte_weight, this}; }

nnh::CellPos walkable_from(const std::uint8_t* cells, int width, int height, nnh::CellPos start) {
    const int n = width * height;
    const int s = start.y * width + start.x;
    for (int k = 0; k < n; ++k) {
        const int i = (s + k) % n;
        if (cells[i] != 0) return {i % width, i / width};
    }
    return start;
}

} // namespace pathfinding_demo
