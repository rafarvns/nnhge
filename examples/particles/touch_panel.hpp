#pragma once
// The bottom screen (spec 024 AC-4): a touch bursts the selected effect at the touched point (emit_at on that effect's
// bottom-screen emitter); L/R select the effect. Logs `burst: <name> <n> at <x>,<y> live <live>` and, once the burst's
// emitter has finished, `done: <name> live 0 finished`.

#include <nnh/particles.hpp>
#include <nnh/texture.hpp>

class TouchPanel {
public:
    static constexpr int kEffects = 3;

    bool create();
    /// L/R and a touch; then the emitters' update.
    void update();
    /// Bursts the selected effect at (x, y) (what a touch does).
    void burst(int x, int y);
    void draw(const nnh::Texture& sheet) const;
    /// Asks for more particles than the selected emitter holds (PARTICLES_SHOT=full).
    void overfill();

    int selected() const { return selected_; }
    static const char* name(int i);
    const nnh::ParticleEmitter& emitter(int i) const { return emitters_[i]; }

private:
    nnh::ParticleEmitter emitters_[kEffects];
    bool waiting_[kEffects] = {}; ///< a burst is still live: log `done:` when it finishes
    int selected_ = 0;
};
