#include "effects.hpp"

namespace effects {
namespace {

using nnh::Frames;
using nnh::ParticleDesc;

void frame(ParticleDesc& d, int i, nnh::Region r) { d.frames[i].region = r; }

} // namespace

ParticleDesc torch() {
    ParticleDesc d;
    d.rate = 40;
    d.space = nnh::ParticleSpace::Local;
    d.shape = nnh::ParticleShape::Line;
    d.length = 6;
    d.angle = -nnh::kQuarterTurn;
    d.spread = 0.5f;
    d.speed = {25, 45};
    d.lifetime = {Frames{24}, Frames{36}};
    d.color_keys[0] = {0, nnh::rgba(255, 255, 200, 255)};
    d.color_keys[1] = {0.35f, nnh::rgba(255, 190, 60, 255)};
    d.color_keys[2] = {0.75f, nnh::rgba(230, 70, 20, 220)};
    d.color_keys[3] = {1, nnh::rgba(120, 30, 10, 0)};
    for (int i = 0; i < 4; ++i) frame(d, i, {8 * i, 8, 8, 8});
    d.frame_mode = nnh::ParticleFrameMode::OverLife;
    d.seed = 11;
    return d;
}

ParticleDesc smoke() {
    ParticleDesc d;
    d.rate = 12;
    d.shape = nnh::ParticleShape::Box;
    d.box = {6, 2};
    d.angle = -nnh::kQuarterTurn;
    d.spread = 0.6f;
    d.speed = {20, 35};
    d.gravity = {12, -10}; // a breeze to the right
    d.damping = 0.8f;
    d.spin = {-1, 1};
    d.rotation = {0, nnh::kFullTurn};
    d.lifetime = {Frames{90}, Frames{140}};
    d.color_min = nnh::rgba(150, 150, 160, 255);
    d.color_max = nnh::rgba(210, 210, 220, 255);
    d.color_keys[0] = {0, nnh::rgba(255, 255, 255, 220)};
    d.color_keys[1] = {1, nnh::rgba(255, 255, 255, 0)};
    d.size = {0.6f, 0.9f};
    d.size_keys[0] = {0, 1};
    d.size_keys[1] = {1, 3.5f};
    frame(d, 0, {8, 0, 8, 8});
    d.seed = 12;
    return d;
}

ParticleDesc rain() {
    ParticleDesc d;
    d.rate = 140;
    d.shape = nnh::ParticleShape::Box;
    d.box = {440, 1};
    d.angle = nnh::kQuarterTurn + 0.15f; // down, a little to the right... of a slanted sky
    d.speed = {260, 320};
    d.lifetime = {Frames{60}, Frames{70}};
    d.color_min = nnh::rgba(170, 190, 255, 200);
    d.color_max = nnh::rgba(210, 225, 255, 255);
    d.rotation = {-0.15f, -0.15f};
    frame(d, 0, {32, 0, 2, 8});
    d.prewarm = 70;
    d.seed = 13;
    return d;
}

ParticleDesc exhaust() {
    ParticleDesc d;
    d.rate = 90;
    d.angle = nnh::kHalfTurn; // backwards from the ship's nose
    d.spread = 0.35f;
    d.speed = {30, 60};
    d.inherit_velocity = 0.3f;
    d.damping = 1.5f;
    d.lifetime = {Frames{30}, Frames{50}};
    d.color_keys[0] = {0, nnh::rgba(200, 240, 255, 255)};
    d.color_keys[1] = {0.4f, nnh::rgba(90, 160, 255, 200)};
    d.color_keys[2] = {1, nnh::rgba(40, 40, 120, 0)};
    d.size = {0.75f, 1};
    d.size_keys[0] = {0, 1};
    d.size_keys[1] = {1, 0.3f};
    frame(d, 0, {16, 0, 8, 8});
    d.seed = 14;
    return d;
}

ParticleDesc explosion() {
    ParticleDesc d;
    d.shape = nnh::ParticleShape::Circle;
    d.radius = 10;
    d.inner_radius = 6;
    d.direction = nnh::ParticleDirection::Outward;
    d.speed = {60, 180};
    d.damping = 2.5f;
    d.spin = {-6, 6};
    d.lifetime = {Frames{30}, Frames{60}};
    d.bursts[0] = {.count = {60, 80}};
    d.bursts[1] = {.at = Frames{6}, .count = {20, 30}};
    d.color_keys[0] = {0, nnh::rgba(255, 255, 220, 255)};
    d.color_keys[1] = {0.3f, nnh::rgba(255, 170, 40, 255)};
    d.color_keys[2] = {1, nnh::rgba(120, 20, 10, 0)};
    d.size = {0.75f, 1.5f};
    frame(d, 0, {0, 0, 8, 8});
    frame(d, 1, {24, 0, 8, 8});
    d.frame_mode = nnh::ParticleFrameMode::Random;
    d.seed = 15;
    return d;
}

ParticleDesc fountain() {
    ParticleDesc d;
    d.rate = 60;
    d.angle = -nnh::kQuarterTurn;
    d.spread = 0.4f;
    d.speed = {110, 140};
    d.gravity = {0, 260};
    d.lifetime = {Frames{45}, Frames{60}};
    d.color_min = nnh::rgba(80, 160, 255, 255);
    d.color_max = nnh::rgba(160, 220, 255, 255);
    d.color_keys[0] = {0.7f, nnh::colors::white};
    d.color_keys[1] = {1, nnh::rgba(255, 255, 255, 0)};
    d.size = {2, 3};
    d.seed = 16;
    return d;
}

ParticleDesc fireworks() {
    ParticleDesc d = explosion();
    d.bursts[1] = {};
    d.bursts[0] = {};
    d.speed = {40, 90};
    d.gravity = {0, 60};
    d.color_min = nnh::rgba(120, 255, 160, 255);
    d.color_max = nnh::rgba(255, 120, 255, 255);
    d.frame_mode = nnh::ParticleFrameMode::First;
    d.frames[0].region = {24, 0, 8, 8};
    d.frames[1].region = {0, 0, -1, -1};
    d.seed = 18;
    return d;
}

} // namespace effects
