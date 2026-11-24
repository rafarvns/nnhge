#include "presets.hpp"

#include <cstdio>
#include <nnh/data.hpp>

#include "demos.hpp"

nnh::ParticleDesc to_desc(const FxPreset& p) {
    nnh::ParticleDesc d;
    d.rate = p.rate;
    d.shape = static_cast<nnh::ParticleShape>(p.shape);
    d.radius = p.radius;
    d.box = {p.radius, p.radius};
    d.length = p.radius;
    d.angle = p.angle;
    d.spread = p.spread;
    d.speed = {p.speed_min, p.speed_max};
    d.gravity = p.gravity;
    d.damping = p.damping;
    d.lifetime = {nnh::Frames{p.life_min}, nnh::Frames{p.life_max}};
    d.color_keys[0] = {0, p.start};
    d.color_keys[1] = {1, p.end};
    d.size = {p.size_min, p.size_max};
    d.size_keys[0] = {0, 1};
    d.size_keys[1] = {1, p.size_end};
    d.frames[0].region = {8 * p.frame, 0, 8, 8};
    d.seed = static_cast<std::uint32_t>(p.seed);
    return d;
}

bool load_preset(const char* name, nnh::ParticleDesc& out) {
    char path[48];
    std::snprintf(path, sizeof path, "fx/%s", name);
    FxPreset p;
    const auto loaded = nnh::data::load(path, p);
    if (!loaded) {
        NNH_LOG_ERROR(example, "%s: %s", path, loaded.error().message);
        return false;
    }
    out = to_desc(p);
    const auto checked = nnh::ParticleEmitter::check(out);
    if (!checked) {
        NNH_LOG_ERROR(example, "%s: %s", path, checked.error().message);
        return false;
    }
    return true;
}
