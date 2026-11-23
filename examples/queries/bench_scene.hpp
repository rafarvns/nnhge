#pragma once
// The queries bench (spec 019 AC-7), three phases measured with the clock (and profile scopes):
//   rays    600 frames of 128 first-hit rays (0-400 px, from a fixed hash) against the room's filtered grid and
//           64 boxes: "queries bench: rays avg X ms worst Y ms (target < 2 ms) PASS|FAIL";
//   points  300 frames of 256 point queries over 256 boxes: "queries bench: points avg X ms, Z us per box
//           (target <= 0.1 us) PASS|FAIL" (plan decision 17: judged per box tested);
//   moves   150 frames of 64 collision::move through collision_grid() then 150 through a filtered grid:
//           "queries bench: move unfiltered X us filtered Y us" (per move; recorded, not judged).
// SELECT goes back to the playground.

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
    void finish_phase();

    const Demos& demos_;
    const room::Room& room_;
    Phase phase_ = Phase::Rays;
    int frame_ = 0;
    nnh::Duration total_{}, worst_{};
    double move_plain_us_ = 0;
    nnh::Rect boxes_[kPointBoxes] = {};
    nnh::collision::Layers layers_[kPointBoxes] = {};
    int sink_ = 0; ///< results summed, so the work isn't optimised away
    char lines_[3][128] = {};
};
