#pragma once
// The playground: a ray from the room's emitter to a target the player moves (Circle Pad / D-Pad), against the
// room's tiles and boxes, in three modes (X): first hit, all hits with the cells crossed, box cast. A toggles the
// water bit of the query mask, B / L / R the red / green / blue box bits, Y "ignore one-way". A touch on the bottom
// screen (the room through a Camera2D at (40, 0)) asks what is under that point. SELECT: self-check; START quits.
//
// Logs (the console script expects them): "queries: mode first|all|cast", "queries: mask 0x1F",
// "queries: one-way ignored|blocks", "queries: first hit ..." when the first hit changes, "under: ..." per touch.

#include "demos.hpp"
#include "room.hpp"

#include <nnh/collision.hpp>
#include <nnh/scene.hpp>

class QueriesScene final : public nnh::Scene {
  public:
    enum class Mode { First, All, Cast };
    static constexpr int kMaxHits = 16;
    static constexpr int kMaxCells = 64;
    static constexpr nnh::Vec2 kCastSize{12, 12};
    static constexpr nnh::Vec2 kBottomCamera{40, 0}; ///< the bottom screen's view of the room

    QueriesScene(const Demos& demos, room::Room& room);

    /// Sets the start state from QUERIES_DEMO (first, all, cast, water, touch).
    void preset();
    void on_enter() override;
    void update() override;
    void draw() override;

    // Read by the HUD.
    Mode mode() const { return mode_; }
    nnh::collision::Layers mask() const { return mask_; }
    bool ignore_one_way() const { return ignore_one_way_; }
    const char* first_line() const { return first_line_; }
    const char* under_line() const { return under_line_; }
    const char* under_cell() const { return under_cell_; }     ///< "cell 7,9 solid, layers 0x2"
    const char* under_detail() const { return under_detail_; } ///< "tile 2 \"water\"; box 3 red"
    bool has_touch() const { return has_touch_; }

  private:
    void handle_buttons();
    void move_target();
    void run_queries();
    void ask_under(nnh::Vec2 world);
    void log_first_hit();
    nnh::collision::QueryFilter filter() const;
    void draw_room(nnh::Screen screen) const;
    void draw_results(nnh::Screen screen) const;

    const Demos& demos_;
    room::Room& room_;
    Mode mode_ = Mode::First;
    nnh::collision::Layers mask_ = room::bits::kEverything;
    bool ignore_one_way_ = false;
    nnh::Vec2 target_{330, 200};

    // This frame's results.
    nnh::collision::RayHit first_{};
    bool has_first_ = false;
    nnh::collision::RayHit hits_[kMaxHits] = {};
    int hit_count_ = 0;
    nnh::collision::CellCrossing cells_[kMaxCells] = {};
    int cell_count_ = 0;
    nnh::collision::RayHit cast_{};
    bool has_cast_ = false;

    bool has_touch_ = false;
    nnh::Vec2 touch_{};
    char first_line_[96] = {};
    char under_line_[96] = {};
    char under_cell_[48] = {};
    char under_detail_[160] = {};
};
