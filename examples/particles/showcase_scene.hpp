#pragma once
// The showcase (spec 024 AC-4): the gallery on the top screen, the touch panel and the HUD (the emitters' stats and
// the particle line of gfx::stats()) on the bottom one. A explosion, X world/local space, B rain behind/in front of the
// tile band, D-Pad Down smoke stop/start, L/R the touch effect, SELECT self-check, Y bench, START quit.

#include <nnh/scene.hpp>

#include "demos.hpp"
#include "gallery.hpp"
#include "touch_panel.hpp"

class ShowcaseScene final : public nnh::Scene {
public:
    explicit ShowcaseScene(const Demos& demos) : demos_(demos) {}
    bool create() { return gallery_.create() && panel_.create(); }
    const nnh::Texture& sheet() const { return gallery_.sheet(); }

    void on_enter() override;
    void update() override;
    void draw() override;

private:
    void shot(int frame);
    void draw_hud() const;

    const Demos& demos_;
    Gallery gallery_;
    TouchPanel panel_;
    int frame_ = 0;
    int explosion_log_ = -1; ///< the frame on which to log the explosion's count (the frame after A)
    bool local_ = false;
    bool rain_front_ = false;
};
