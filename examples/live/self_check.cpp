// The example's self-check (see self_check.hpp). Built with -ffp-contract=off.

#include "self_check.hpp"

#include <cstring>

namespace {

struct Fold {
    std::uint32_t h = 0x811C9DC5u;
    void bytes(const void* p, std::size_t n) {
        const auto* b = static_cast<const std::uint8_t*>(p);
        for (std::size_t i = 0; i < n; ++i) {
            h ^= b[i];
            h *= 0x01000193u;
        }
    }
    void f(float v) { bytes(&v, 4); }
    void u(std::uint32_t v) { bytes(&v, 4); }
};

void fold_tuning(Fold& f, const Tuning& t) {
    f.f(t.jump);
    f.f(t.gravity);
    f.f(t.run);
    f.u(t.hitboxes ? 1u : 0u);
    f.u(static_cast<std::uint32_t>(t.count));
    f.u(static_cast<std::uint32_t>(t.shape));
    f.bytes(&t.sky, 4);
    f.bytes(&t.ground, 4);
    f.f(t.dust);
}

} // namespace

std::uint32_t live_self_check(const Tuning& tuning) {
    Fold f;
    fold_tuning(f, tuning);
    World w;
    sim_respawn(w, tuning);
    for (int frame = 0; frame < 600; ++frame) {
        const int dir = (frame / 90) % 3 - 1;
        sim_step(w, tuning, dir, frame % 45 == 0);
        f.f(w.pos.x);
        f.f(w.pos.y);
        f.f(w.vel.x);
        f.f(w.vel.y);
        f.u(static_cast<std::uint32_t>(w.state));
        for (int i = 0; i < w.ball_count; ++i) {
            f.f(w.balls[i].pos.x);
            f.f(w.balls[i].pos.y);
        }
    }
    f.u(static_cast<std::uint32_t>(w.landings));
    return f.h;
}

Tuning live_script_tuning() {
    Tuning t;
    t.jump = 6.25f;
    t.gravity = 0.25f;
    t.run = 2.5f;
    t.count = 9;
    t.shape = Shape::Diamond;
    t.sky = {255, 192, 64, 255};
    t.ground = {32, 96, 48, 255};
    t.dust = 20;
    return t;
}
