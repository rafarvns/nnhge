#include "bench_scene.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>

namespace {

namespace collision = nnh::collision;

constexpr double kRayTargetMs = 2.0;
constexpr double kPointTargetUs = 0.1;

/// A fixed hash of (frame, index): the same rays on every system.
std::uint32_t hash(int frame, int i) {
    std::uint32_t h = static_cast<std::uint32_t>(frame) * 2654435761u ^ static_cast<std::uint32_t>(i) * 40503u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}

/// Unit directions on 32 steps of the circle, short binary fractions (no sin/cos at run time).
nnh::Vec2 direction(std::uint32_t h) {
    static constexpr nnh::Vec2 kEighth[8] = {{1, 0},       {0.9375f, 0.375f}, {0.6875f, 0.6875f}, {0.375f, 0.9375f},
                                             {0, 1},       {-0.375f, 0.9375f}, {-0.6875f, 0.6875f}, {-0.9375f, 0.375f}};
    const nnh::Vec2 d = kEighth[h % 8];
    return (h & 8u) != 0 ? nnh::Vec2{-d.x, -d.y} : d;
}

/// Ray i of a frame: 0-400 px from a point of the room.
struct BenchRay {
    nnh::Vec2 from, to, dir;
};
BenchRay bench_ray(int frame, int i) {
    const std::uint32_t h = hash(frame, i);
    BenchRay r;
    r.from = {static_cast<float>(16 + h % 368), static_cast<float>(16 + (h >> 9) % 208)};
    r.dir = direction(h >> 18);
    r.to = r.from + r.dir * static_cast<float>((h >> 22) % 401);
    return r;
}

/// Cells the grid walk visits up to the ray's hit (all it crosses when nothing blocks).
int cells_visited(const collision::Grid& grid, const BenchRay& r) {
    constexpr int kMax = 80; // 400 px through 16 px cells: at most 52
    collision::CellCrossing crossed[kMax];
    const int total = collision::cells_crossed(grid, r.from, r.to, crossed, kMax);
    collision::RayHit hit;
    if (!collision::raycast(grid, r.from, r.to, hit)) return total;
    for (int i = 0; i < total && i < kMax; ++i) {
        if (crossed[i].x == hit.cell.x && crossed[i].y == hit.cell.y) return i + 1;
    }
    return total;
}

double ms_per_frame(nnh::Duration d, int frames) { return d.seconds() * 1000.0 / frames; }

} // namespace

void BenchScene::on_enter() {
    phase_ = Phase::Rays;
    frame_ = 0;
    total_ = worst_ = grid_rays_ = box_rays_ = start_rays_ = nnh::Duration{};
    cells_ = 0;
    for (auto& line : lines_) line[0] = '\0';
    for (int i = 0; i < kPointBoxes; ++i) {
        const std::uint32_t h = hash(7, i);
        boxes_[i] = {static_cast<float>(16 + h % 360), static_cast<float>(16 + (h >> 9) % 200),
                     static_cast<float>(4 + (h >> 17) % 28), static_cast<float>(4 + (h >> 22) % 28)};
        layers_[i] = collision::layer(static_cast<int>((h >> 27) % 5));
    }
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    NNH_LOG_INFO(example, "queries bench: started (rays, points, moves: about 20 s)");
}

void BenchScene::run_frame() {
    const collision::Grid grid = room_.grid(room::bits::kWorld); // a filtered grid: walls and planks, no water
    switch (phase_) {
    case Phase::Rays: {
        NNH_PROFILE_SCOPE("queries rays");
        const collision::BoxView view = collision::box_view(boxes_, layers_, kRayBoxes);
        for (int i = 0; i < kRaysPerFrame; ++i) {
            const BenchRay r = bench_ray(frame_, i);
            collision::RayHit hit;
            sink_ += collision::raycast(grid, view, r.from, r.to, hit) ? hit.cell.x : 0;
        }
        break;
    }
    case Phase::Points: {
        NNH_PROFILE_SCOPE("queries points");
        const collision::BoxView view = collision::box_view(boxes_, layers_, kPointBoxes);
        int found[8];
        for (int i = 0; i < kPointsPerFrame; ++i) {
            const std::uint32_t h = hash(frame_, i);
            const nnh::Vec2 p{static_cast<float>(h % 400), static_cast<float>((h >> 9) % 240)};
            sink_ += collision::boxes_at(view, p, found, 8, {.mask = 0x7u});
        }
        break;
    }
    default: {
        NNH_PROFILE_SCOPE("queries moves");
        const collision::Grid g = phase_ == Phase::MovesPlain ? room_.map.collision_grid() : grid;
        for (int i = 0; i < kMovesPerFrame; ++i) {
            const std::uint32_t h = hash(frame_, i);
            const nnh::Rect box{static_cast<float>(24 + h % 340), static_cast<float>(24 + (h >> 9) % 180), 12, 14};
            const collision::MoveResult r = collision::move(g, box, direction(h >> 18) * 24.0f);
            sink_ += r.touching.floor ? 1 : 0;
        }
        break;
    }
    }
}

void BenchScene::run_ray_split() {
    const collision::Grid grid = room_.grid(room::bits::kWorld);
    const collision::BoxView view = collision::box_view(boxes_, layers_, kRayBoxes);
    collision::RayHit hit;
    nnh::Duration t = nnh::time::now();
    for (int i = 0; i < kRaysPerFrame; ++i) {
        const BenchRay r = bench_ray(frame_, i);
        sink_ += collision::raycast(grid, r.from, r.to, hit) ? hit.cell.x : 0;
    }
    nnh::Duration now = nnh::time::now();
    grid_rays_ += now - t;
    t = now;
    for (int i = 0; i < kRaysPerFrame; ++i) {
        const BenchRay r = bench_ray(frame_, i);
        sink_ += collision::raycast(view, r.from, r.to, hit) ? hit.box : 0;
    }
    now = nnh::time::now();
    box_rays_ += now - t;
    t = now;
    for (int i = 0; i < kRaysPerFrame; ++i) { // the walk's setup and its first cell
        const BenchRay r = bench_ray(frame_, i);
        sink_ += collision::raycast(grid, r.from, r.from + r.dir * 0.25f, hit) ? hit.cell.x : 0;
    }
    start_rays_ += nnh::time::now() - t;
    for (int i = 0; i < kRaysPerFrame; ++i) cells_ += cells_visited(grid, bench_ray(frame_, i));
}

void BenchScene::finish_rays() {
    const double avg = ms_per_frame(total_, kRayFrames);
    rays_pass_ = avg < kRayTargetMs;
    std::snprintf(lines_[0], sizeof lines_[0], "queries bench: rays avg %.3f ms worst %.3f ms (target < 2 ms) %s",
                  avg, worst_.seconds() * 1000.0, rays_pass_ ? "PASS" : "FAIL");
    NNH_LOG_INFO(example, "%s", lines_[0]);
    std::snprintf(lines_[1], sizeof lines_[1],
                  "queries bench: rays split grid %.3f ms (start %.3f ms, %.1f cells per ray) boxes %.3f ms",
                  ms_per_frame(grid_rays_, kRayFrames), ms_per_frame(start_rays_, kRayFrames),
                  static_cast<double>(cells_) / (static_cast<double>(kRayFrames) * kRaysPerFrame),
                  ms_per_frame(box_rays_, kRayFrames));
    NNH_LOG_INFO(example, "%s", lines_[1]);
}

void BenchScene::finish_phase() {
    const double ms = total_.seconds() * 1000.0;
    if (phase_ == Phase::Rays) {
        finish_rays();
        phase_ = Phase::Points;
    } else if (phase_ == Phase::Points) {
        const double avg = ms / kPointFrames;
        const double per_box_us = avg * 1000.0 / (static_cast<double>(kPointsPerFrame) * kPointBoxes);
        points_pass_ = per_box_us <= kPointTargetUs;
        std::snprintf(lines_[2], sizeof lines_[2],
                      "queries bench: points avg %.3f ms, %.4f us per box (target <= 0.1 us) %s", avg, per_box_us,
                      points_pass_ ? "PASS" : "FAIL");
        NNH_LOG_INFO(example, "%s", lines_[2]);
        phase_ = Phase::MovesPlain;
    } else if (phase_ == Phase::MovesPlain) {
        move_plain_us_ = ms * 1000.0 / (static_cast<double>(kMoveFrames) * kMovesPerFrame);
        phase_ = Phase::MovesFiltered;
    } else {
        const double filtered = ms * 1000.0 / (static_cast<double>(kMoveFrames) * kMovesPerFrame);
        std::snprintf(lines_[3], sizeof lines_[3], "queries bench: move unfiltered %.2f us filtered %.2f us",
                      move_plain_us_, filtered);
        NNH_LOG_INFO(example, "%s (sink %d)", lines_[3], sink_ & 1);
        if (rays_pass_ && points_pass_) {
            NNH_LOG_INFO(example, "queries bench: PASS");
        } else { // check.nnhs fails on this line, after every number is logged
            NNH_LOG_INFO(example, "queries bench: FAIL (%s%s%s)", rays_pass_ ? "" : "rays",
                         !rays_pass_ && !points_pass_ ? ", " : "", points_pass_ ? "" : "points");
        }
        phase_ = Phase::Done;
    }
    frame_ = 0;
    total_ = worst_ = nnh::Duration{};
}

void BenchScene::update() {
    if (phase_ != Phase::Done) {
        const nnh::Duration start = nnh::time::now();
        run_frame();
        const nnh::Duration spent = nnh::time::now() - start;
        total_ += spent;
        if (spent > worst_) worst_ = spent;
        if (phase_ == Phase::Rays) run_ray_split(); // timed apart: not in the judged time
        const int frames = phase_ == Phase::Rays ? kRayFrames : (phase_ == Phase::Points ? kPointFrames : kMoveFrames);
        if (++frame_ == frames) finish_phase();
    }
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.play);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void BenchScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 30}, "QUERIES BENCH", {.scale = 2, .align = nnh::TextAlign::Center});
    const char* names[] = {"rays", "points", "moves (plain)", "moves (filtered)", "done"};
    char info[64];
    std::snprintf(info, sizeof info, "phase: %s, frame %d", names[static_cast<int>(phase_)], frame_);
    gfx::draw_text(nnh::Screen::Top, {200, 80}, info, {.align = nnh::TextAlign::Center});
    for (int i = 0; i < 4; ++i) {
        char shown[128] = "...";
        if (lines_[i][0] != '\0') {
            std::snprintf(shown, sizeof shown, "%s", lines_[i] + 15); // without "queries bench: "
            char* cut = std::strstr(shown, " (");
            if (cut == nullptr) cut = std::strstr(shown, " filtered");
            if (i == 1) { // the split: three lines (grid, start, boxes)
                cut = std::strstr(shown, " (");
                char* boxes = std::strstr(shown, " boxes");
                if (boxes != nullptr) *boxes = '\n';
            }
            if (cut != nullptr) *cut = '\n'; // the second half on its own line: 40 characters fit the screen
        }
        static constexpr float kTop[4] = {8, 36, 78, 106}; // the split takes three text lines
        gfx::draw_text(nnh::Screen::Bottom, {4, kTop[i]}, shown,
                       {.color = nnh::colors::yellow});
    }
}
