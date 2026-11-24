// The pathfinding self-check (spec 025 AC-3), see self_check.hpp.

#include "self_check.hpp"

#include <memory>

namespace pathfinding_demo {

using nnh::CellPos;
using nnh::PathResult;

namespace {

std::uint32_t u(int v) { return static_cast<std::uint32_t>(v); }

CellPos random_cell(std::uint32_t h, int w, int hgt) {
    return {static_cast<int>(h % u(w)), static_cast<int>((h >> 16) % u(hgt))};
}

/// The first blocked cell at or after a random one (a goal nobody can stand on), or the cell itself.
CellPos blocked_from(const std::uint8_t* bytes, int w, int h, CellPos start) {
    for (int k = 0; k < w * h; ++k) {
        const int i = (start.y * w + start.x + k) % (w * h);
        if (bytes[i] == 0) return {i % w, i / w};
    }
    return start;
}

} // namespace

bool SelfCheck::init() {
    rooms(64, 64, 1, m1_);
    maze(48, 48, 2, m2_, scratch_);
    swamp(32, 24, 3, m3_);
    for (int i = 0; i < 64 * 64; ++i) m4_[i] = m1_[i];
    std::uint8_t* bytes[4] = {m1_, m2_, m3_, m4_};
    const int sizes[4][3] = {{64, 64, 1}, {48, 48, 1}, {32, 24, 1}, {64, 64, 2}};
    for (int k = 0; k < 4; ++k) {
        Level& l = levels_[k];
        l.bytes = bytes[k];
        l.width = sizes[k][0];
        l.height = sizes[k][1];
        l.source = {l.bytes, l.width, l.height};
        auto map = nnh::PathMap::create({.width = l.width, .height = l.height, .agent_size = sizes[k][2]});
        if (!map) return false;
        l.map = static_cast<nnh::PathMap&&>(map.value());
        l.map.sync(l.source.grid(), l.source.weights());
    }
    auto searcher = nnh::PathSearcher::create(64, 64);
    auto field = nnh::DistanceField::create({.width = 64, .height = 64});
    if (!searcher || !field) return false;
    searcher_ = static_cast<nnh::PathSearcher&&>(searcher.value());
    field_ = static_cast<nnh::DistanceField&&>(field.value());
    return true;
}

void SelfCheck::mix(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        sum_ ^= (v >> (8 * i)) & 0xFFu;
        sum_ *= 16777619u;
    }
}

int SelfCheck::run(int n) {
    int ran = 0;
    while (ran < n && !done()) {
        search(done_);
        ++done_;
        ++ran;
        if (done_ % kCheckpointEvery == 0) checkpoints_[done_ / kCheckpointEvery - 1] = sum_;
    }
    return ran;
}

void SelfCheck::search(int i) {
    Level& l = levels_[i % 4];
    const std::uint32_t h = hash4(u(i), 0xC0FFEEu, 0, 1);
    l.map.set_diagonals(static_cast<nnh::Diagonals>((i / 4) % 4));
    const CellPos from = walkable_from(l.bytes, l.width, l.height, random_cell(hash4(u(i), 1, 0, 2), l.width, l.height));
    CellPos to = walkable_from(l.bytes, l.width, l.height, random_cell(hash4(u(i), 2, 0, 2), l.width, l.height));
    switch ((h >> 3) % 8) {
    case 5: to = blocked_from(l.bytes, l.width, l.height, to); break;
    case 6: to = from; break;
    case 7: to = {l.width / 2 - 1, l.height / 2 - 1}; break; // M3: inside the island
    default: break;
    }
    static const int kLimits[3] = {0, 50, 200};
    static const int kScales[3] = {16, 24, 64};
    static const int kSlices[5] = {0, 1, 7, 64, 256};
    const nnh::PathOptions o{.max_expanded = kLimits[(h >> 6) % 3],
                             .partial = ((h >> 8) & 1u) != 0,
                             .heuristic_scale = kScales[(h >> 9) % 3]};
    const int slice = kSlices[(h >> 11) % 5];
    if (i % 64 == 63) {
        stale_search(i, l, from, to);
    } else if (slice == 0) {
        searcher_.find(l.map, from, to, o);
    } else {
        searcher_.begin(l.map, from, to, o);
        while (!searcher_.step(slice)) {
        }
    }
    const PathResult r = searcher_.result();
    mix(u(static_cast<int>(r.status)) | (r.stale ? 0x100u : 0u));
    mix(r.cost);
    mix(u(r.cells));
    mix(u(r.expanded));
    const int n = searcher_.path(cells_, 64 * 64);
    for (int k = 0; k < n && k < 64 * 64; ++k) mix((u(cells_[k].y) << 16) | u(cells_[k].x));
    if (i % 8 == 0) smooth(l, n);
    if (i % 32 == 16) field(i, l);
    if (i % 32 == 31) door(i, l);
}

void SelfCheck::stale_search(int i, Level& l, CellPos from, CellPos to) {
    searcher_.begin(l.map, from, to, {.partial = true});
    searcher_.step(5);
    const CellPos c = random_cell(hash4(u(i), 3, 0, 2), l.width, l.height);
    l.map.set_cell(c.x, c.y, l.map.blocked(c.x, c.y) ? 2 : nnh::kPathBlocked);
    while (!searcher_.step(64)) {
    }
}

void SelfCheck::smooth(Level& l, int count) {
    const int n = nnh::smooth_path(l.map, cells_, count, waypoints_, 64 * 64);
    mix(u(n));
    for (int k = 0; k < n && k < 64 * 64; ++k) mix((u(waypoints_[k].y) << 16) | u(waypoints_[k].x));
}

void SelfCheck::door(int i, Level& l) {
    const CellPos c = walkable_from(l.bytes, l.width, l.height, random_cell(hash4(u(i), 4, 0, 2), l.width, l.height));
    switch ((i / 32) % 3) {
    case 0: l.map.fill({c.x - 1, c.y - 1, 3, 3}, (i / 32) % 2 == 0 ? nnh::kPathBlocked : 5); break;
    case 1: l.map.set_cell(c.x, c.y, nnh::kPathBlocked); break;
    default: // a door in the source cells: toggled, then a region sync from the callback grid
        for (int y = c.y - 1; y <= c.y + 1; ++y) {
            for (int x = c.x - 1; x <= c.x + 1; ++x) {
                if (x < 0 || y < 0 || x >= l.width || y >= l.height) continue;
                std::uint8_t& b = l.bytes[y * l.width + x];
                b = b == 0 ? 1 : 0;
            }
        }
        l.map.sync(l.source.grid(), {c.x - 1, c.y - 1, 3, 3}, l.source.weights());
        break;
    }
    mix(l.map.version());
}

void SelfCheck::field(int i, Level& l) {
    const int k = i / 32;
    CellPos goals[8];
    const int count = 1 + k % 8;
    for (int g = 0; g < count; ++g) {
        goals[g] = walkable_from(l.bytes, l.width, l.height, random_cell(hash4(u(i), 5, u(g), 2), l.width, l.height));
    }
    const std::uint32_t max_cost = k % 3 == 0 ? 300u : nnh::kUnreachable;
    if (k % 2 == 0) {
        field_.build(l.map, goals, count, max_cost);
    } else {
        field_.begin(l.map, goals, count, max_cost);
        while (!field_.step(37)) {
        }
    }
    mix(u(field_.settled()));
    for (int y = 0; y < l.height; ++y) {
        for (int x = 0; x < l.width; ++x) mix(field_.distance({x, y}));
    }
    for (int q = 0; q < 64; ++q) {
        const CellPos at = random_cell(hash4(u(i), 6, u(q), 2), l.width, l.height);
        CellPos n{-1, -1};
        const bool has = field_.next(l.map, at, n);
        mix(has ? (u(n.y) << 16) | u(n.x) : 0xFFFFFFFFu);
    }
}

std::uint32_t run_self_check(std::uint32_t* checkpoints) {
    auto check = std::make_unique<SelfCheck>();
    if (!check->init()) return 0;
    check->run(kCheckSearches);
    if (checkpoints != nullptr) {
        for (int k = 0; k < kCheckpoints; ++k) checkpoints[k] = check->checkpoint(k);
    }
    return check->checksum();
}

} // namespace pathfinding_demo

namespace pathfinding_demo {

// Recorded from pc-debug (spec 025 T6.2); a changed tie rule, neighbour order or cost changes them (by design).
const std::uint32_t kPathfindingCheckGolden = 0x1C0B5992u;
const std::uint32_t kPathfindingCheckpoints[kCheckpoints] = {0x8332890Fu, 0xBF55A00Bu, 0xB3E6C49Au, 0xC825A5E7u,
                                                           0x77C6FFC5u, 0xE738F010u, 0x528F7687u, 0x1C0B5992u};

} // namespace pathfinding_demo
