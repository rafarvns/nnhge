#pragma once
// Switches between the example's demos (scenes): SELECT goes to the next one (level 1 → bonus level →
// benchmark → flip test → level 1), START quits, and ShowDemo requests (a level's door, the replay) go
// straight to one. It listens to events only (ButtonPressed, ShowDemo): the scenes never know each other.

#include "game_events.hpp"
#include "replay.hpp"

#include <nnh/events.hpp>
#include <nnh/input.hpp>
#include <nnh/scene.hpp>

class Demos {
  public:
    static constexpr int kCount = 4;

    /// `scenes` in Demo order; they and `replay` must outlive this object.
    Demos(nnh::Scene* const (&scenes)[kCount], Replay& replay);

    /// The scene of `demo`, to start nnh::run() with; it becomes the current demo.
    nnh::Scene& first(Demo demo);

  private:
    void on_button(const nnh::ButtonPressed& e);
    void on_show(const ShowDemo& e);
    void show(Demo demo);

    nnh::Scene* scenes_[kCount] = {};
    Replay& replay_;
    Demo current_ = Demo::Level1;
    nnh::Subscription button_sub_;
    nnh::Subscription show_sub_;
};
