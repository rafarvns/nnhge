// The overlay (spec 023 FR-7, FR-8): after every scene's draw, each screen's effect for the phase — built-in effects
// as rectangles (gfx::fill_rect, layer 15, ignore_camera: above every game layer, below the debug pass, never
// zoomed), custom ones through Options::custom (called every transition frame, coverage 0 too).

#include "transition/transition_internal.hpp"

namespace nnh::detail {

void transition_draw_overlay(const TransitionState& t) {
    const transition::Phase phase = t.timeline.phase;
    const int in = phase == transition::Phase::In ? 1 : 0;
    const EffectArea area{t.tall, t.gap};
    const int q = coverage_q(t.timeline.coverage);
    for (Screen screen : {Screen::Top, Screen::Bottom}) {
        const ResolvedEffect& fx = t.fx[(screen == Screen::Bottom ? kBottomOut : kTopOut) + in];
        if (fx.kind == transition::Kind::Custom) {
            if (t.custom) {
                const int frame = phase == transition::Phase::Covered ? static_cast<int>(t.timeline.covered)
                                                                       : static_cast<int>(t.timeline.k);
                t.custom(transition::CustomFrame{screen, t.timeline.coverage, phase, frame, t.loading_progress,
                                                 t.tall});
            }
            continue;
        }
        draw_resolved(fx, screen, area, q, 15);
    }
}

} // namespace nnh::detail
