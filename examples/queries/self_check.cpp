#include "self_check.hpp"

#include <cstdio>
#include <cstring>

namespace {

namespace collision = nnh::collision;
using collision::Cell;

} // namespace

QueriesSelfCheck::QueriesSelfCheck() {
    for (Cell& c : cells_) {
        const std::uint32_t r = next() % 100;
        c = r < 18 ? Cell::Solid : (r < 28 ? Cell::OneWay : Cell::Empty);
    }
    open_ = collision::grid_view(cells_, kGridW, kGridH, 16, 12, {-8.5f, 4.25f});
    walled_ = open_;
    walled_.outside = Cell::Solid;
    for (int i = 0; i < kBoxes; ++i) {
        const float x = static_cast<float>(static_cast<int>(next() % 7200)) / 16 - 40;
        const float y = static_cast<float>(static_cast<int>(next() % 4800)) / 16 - 30;
        const float w = i % 13 == 0 ? 0 : static_cast<float>(4 + next() % 36);
        const float h = static_cast<float>(4 + next() % 36) + (i % 7 == 0 ? 0.375f : 0);
        boxes_[i] = {x, y, w, h};
        layers_[i] = i % 11 == 0 ? collision::kNoLayers : collision::layer(static_cast<int>(next() % 5));
    }
}

std::uint32_t QueriesSelfCheck::next() { // xorshift32: the same sequence everywhere
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return state_;
}

nnh::Vec2 QueriesSelfCheck::point() {
    const std::uint32_t kind = next() % 8;
    if (kind == 0) { // on grid lines and corners
        return {-8.5f + static_cast<float>(static_cast<int>(next() % 27) - 1) * 16,
                4.25f + static_cast<float>(static_cast<int>(next() % 19) - 1) * 12};
    }
    if (kind == 1) { // anywhere, not on the 1/16 lattice
        return {static_cast<float>(static_cast<int>(next() % 60000)) / 97.0f - 100,
                static_cast<float>(static_cast<int>(next() % 40000)) / 89.0f - 80};
    }
    if (kind == 2) { // far away
        return {static_cast<float>(static_cast<int>(next() % 8000)) - 4000,
                static_cast<float>(static_cast<int>(next() % 6000)) - 3000};
    }
    return {static_cast<float>(static_cast<int>(next() % 8000)) / 16 - 60,
            static_cast<float>(static_cast<int>(next() % 5600)) / 16 - 50};
}

collision::QueryFilter QueriesSelfCheck::filter() {
    const std::uint32_t r = next();
    collision::QueryFilter f;
    if (r % 3 == 0) f.mask = collision::layer(static_cast<int>(r / 3 % 5)) | collision::layer(static_cast<int>(r / 17 % 5));
    f.ignore = static_cast<int>(r >> 20) % 80 - 8;
    f.hit_from_inside = (r & 0x100u) != 0;
    f.ignore_one_way = (r & 0x200u) != 0;
    return f;
}

void QueriesSelfCheck::mix(std::uint32_t word) {
    for (int i = 0; i < 4; ++i) {
        sum_ ^= (word >> (8 * i)) & 0xFFu;
        sum_ *= 16777619u;
    }
}

void QueriesSelfCheck::mix_float(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    mix(bits);
}

void QueriesSelfCheck::mix_hit(const collision::RayHit& h) {
    mix_float(h.fraction);
    mix_float(h.distance);
    mix_float(h.point.x);
    mix_float(h.point.y);
    mix_float(h.normal.x);
    mix_float(h.normal.y);
    mix(static_cast<std::uint32_t>(h.box));
    mix(static_cast<std::uint32_t>(h.cell.x));
    mix(static_cast<std::uint32_t>(h.cell.y));
    mix(static_cast<std::uint32_t>(h.cell.kind) | (h.inside ? 0x100u : 0u));
}

void QueriesSelfCheck::ray() {
    const nnh::Vec2 from = point();
    nnh::Vec2 to = point();
    const std::uint32_t shape = next() % 10;
    if (shape == 0) to.x = from.x; // vertical
    if (shape == 1) to.y = from.y; // horizontal
    if (shape == 2) to = from + nnh::Vec2{32, 24}; // through corners from a corner
    if (shape == 3) to = from;                       // zero length
    const collision::QueryFilter f = filter();
    const collision::Grid& grid = (rays_ % 2 == 0) ? open_ : walled_;
    const collision::BoxView view = collision::box_view(boxes_, layers_, kBoxes);
    collision::RayHit hit, hits[8];
    collision::CellCrossing crossed[32];
    const bool cell = collision::raycast(grid, from, to, hit, f);
    mix(cell ? 1u : 0u);
    mix_hit(hit);
    stats_.grid_hits += cell ? 1 : 0;
    stats_.inside_hits += cell && hit.inside ? 1 : 0;
    stats_.one_way_hits += cell && hit.cell.kind == collision::Cell::OneWay ? 1 : 0;
    const bool box = collision::raycast(view, from, to, hit, f);
    mix(box ? 1u : 0u);
    mix_hit(hit);
    stats_.box_hits += box ? 1 : 0;
    stats_.inside_hits += box && hit.inside ? 1 : 0;
    mix(collision::raycast(grid, view, from, to, hit, f) ? 1u : 0u);
    mix_hit(hit);
    for (int pass = 0; pass < 2; ++pass) {
        const int n = pass == 0 ? collision::raycast_all(grid, from, to, hits, 8, f)
                                : collision::raycast_all(view, from, to, hits, 8, f);
        mix(static_cast<std::uint32_t>(n));
        for (int i = 0; i < n && i < 8; ++i) mix_hit(hits[i]);
    }
    const int n = collision::cells_crossed(grid, from, to, crossed, 32);
    mix(static_cast<std::uint32_t>(n));
    for (int i = 0; i < n && i < 32; ++i) {
        mix(static_cast<std::uint32_t>(crossed[i].x));
        mix(static_cast<std::uint32_t>(crossed[i].y));
        mix(static_cast<std::uint32_t>(crossed[i].kind));
        mix_float(crossed[i].fraction);
    }
    const bool clear = collision::line_of_sight(grid, view, from, to, f);
    mix(clear ? 1u : 0u);
    stats_.blocked_sights += clear ? 0 : 1;
}

void QueriesSelfCheck::cast() {
    const nnh::Vec2 at = point();
    const nnh::Rect box{at.x, at.y, static_cast<float>(1 + next() % 30), static_cast<float>(1 + next() % 30) / 2};
    const nnh::Vec2 to = point();
    const collision::QueryFilter f = filter();
    const collision::BoxView view = collision::box_view(boxes_, layers_, kBoxes);
    collision::RayHit hit, hits[8];
    const bool met = collision::box_cast(view, box, to, hit, f);
    mix(met ? 1u : 0u);
    stats_.cast_hits += met ? 1 : 0;
    mix_hit(hit);
    const int n = collision::box_cast_all(view, box, to, hits, 8, f);
    mix(static_cast<std::uint32_t>(n));
    for (int i = 0; i < n && i < 8; ++i) mix_hit(hits[i]);
}

bool QueriesSelfCheck::step() {
    if (done()) return false;
    if (rays_ < kRays) {
        ray();
        ++rays_;
        return rays_ % kCheckpointEvery == 0;
    }
    cast();
    ++casts_;
    return false;
}

void format_queries_checkpoint(char* out, std::size_t size, int rays, std::uint32_t sum) {
    std::snprintf(out, size, "queries: %d sum %08X", rays, static_cast<unsigned>(sum));
}

void format_queries_verdict(char* out, std::size_t size, std::uint32_t sum) {
    if (sum == kQueriesCheckGolden) {
        std::snprintf(out, size, "queries: PASS %08X", static_cast<unsigned>(sum));
    } else {
        std::snprintf(out, size, "queries: FAIL sum %08X expected %08X", static_cast<unsigned>(sum),
                      static_cast<unsigned>(kQueriesCheckGolden));
    }
}
