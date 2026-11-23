#pragma once
// The queries bench (spec 019 AC-7), three phases measured with the clock (and profile scopes):
//   rays    600 frames of 128 first-hit rays (0-400 px, from a fixed hash) against the room's filtered grid and
//           64 boxes: "queries bench: rays avg X ms worst Y ms (target < 2 ms) PASS|FAIL"; then the same rays split,
//           timed apart (not in the target): "queries bench: rays split grid G ms (start S ms, C cells per ray)
//           boxes B ms" per frame (start: the same rays cut to 1/4 px, the walk's setup and one cell);
//   points  300 frames of 256 point queries over 256 boxes: "queries bench: points avg X ms, Z us per box
//           (target <= 0.1 us) PASS|FAIL" (plan decision 17: judged per box tested);
//   moves   150 frames of 64 collision::move through collision_grid() then 150 through a filtered grid:
//           "queries bench: move unfiltered X us filtered Y us" (per move; recorded, not judged).
// Then the verdict of both targets: "queries bench: PASS" or "queries bench: FAIL (rays, points)" (check.nnhs fails
// on it, so one console run reports every number). SELECT goes back to the playground.

#include "demos.hpp"
#include "room.hpp"

#include <nnh/collision.hpp>
#include <nnh/scene.hpp>
#include <nnh/time.hpp>

class BenchScene final : public nnh::Scene {
  public:
    static constexpr int kRayFrames = 600, kRaysPerFrame = 128, kRayBoxes = 64;
    static constexpr int kPointFrames = 300, kPointsPerFrame = 256, kPointBoxes = 256;
    static constexpr int kMoveFrames = 150, kMovesPerFrame = 64;

    BenchScene(const Demos& demos, const room::Room& room) : demos_(demos), room_(room) {}

    void on_enter() override;
    void update() override;
    void draw() override;

  private:
    enum class Phase { Rays, Points, MovesPlain, MovesFiltered, Done };
    void run_frame();
    void run_ray_split(); ///< the rays phase's grid / boxes / start timings (outside the judged time)
    void finish_phase();
    void finish_rays();

    const Demos& demos_;
    const room::Room& room_;
    Phase phase_ = Phase::Rays;
    int frame_ = 0;
    nnh::Duration total_{}, worst_{};
    nnh::Duration grid_rays_{}, box_rays_{}, start_rays_{}; ///< the rays split, summed over the phase
    long long cells_ = 0;                      ///< cells the grid rays visit, summed over the phase
    bool rays_pass_ = false, points_pass_ = false;
    double move_plain_us_ = 0;
    nnh::Rect boxes_[kPointBoxes] = {};
    nnh::collision::Layers layers_[kPointBoxes] = {};
    int sink_ = 0; ///< results summed, so the work isn't optimised away
    char lines_[4][128] = {}; ///< rays, rays split, points, moves (shown on the bottom screen)
};
