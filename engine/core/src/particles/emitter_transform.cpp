// Where an emitter is (spec 024 FR-14): its transform and the previous update's (sub-frame spreading, inherited
// velocity), a `jump` that cancels both across a teleport, non-finite values refused (an integer test), bounds.

#include "particles/particle_bits.hpp"
#include "particles/particle_block.hpp"
#include "transform/sin_cos.hpp"

namespace nnh {
namespace detail {

bool finite_world(const WorldTransform2D& t) {
    const float v[9] = {t.map.x.x, t.map.x.y, t.map.y.x, t.map.y.y, t.map.origin.x, t.map.origin.y,
                        t.rotation, t.scale.x, t.scale.y};
    for (float f : v) {
        if (!finite_float(f)) return false;
    }
    return true;
}

namespace {

void place(ParticleBlock& b, const WorldTransform2D& t, bool jump) {
    b.now = t;
    if (jump) b.before = t;
}

} // namespace
} // namespace detail

void ParticleEmitter::set_transform(const Transform2D& t, bool jump) {
    if (block_ == nullptr || !detail::is_finite_bits(t)) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::set_transform: an empty emitter or a non-finite transform");
        return;
    }
    detail::place(*block_, to_world(t), jump);
}

void ParticleEmitter::set_transform(const WorldTransform2D& t, bool jump) {
    if (block_ == nullptr || !detail::finite_world(t)) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::set_transform: an empty emitter or a non-finite transform");
        return;
    }
    detail::place(*block_, t, jump);
}

void ParticleEmitter::set_position(Vec2 position, bool jump) {
    if (block_ == nullptr || !detail::finite_float(position.x) || !detail::finite_float(position.y)) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::set_position: an empty emitter or a non-finite position");
        return;
    }
    WorldTransform2D t = block_->now;
    t.map.origin = position;
    detail::place(*block_, t, jump);
}

const WorldTransform2D& ParticleEmitter::transform() const {
    static constexpr WorldTransform2D kIdentity{};
    return block_ != nullptr ? block_->now : kIdentity;
}

Rect ParticleEmitter::bounds() const {
    const detail::ParticleBlock* b = block_;
    if (b == nullptr) return {};
    if (b->alive == 0) return {b->now.map.origin.x, b->now.map.origin.y, 0, 0};
    const float x0 = detail::key_float(b->key_min_x), y0 = detail::key_float(b->key_min_y);
    const Rect r{x0, y0, detail::key_float(b->key_max_x) - x0, detail::key_float(b->key_max_y) - y0};
    return b->derived.world ? r : nnh::bounds(b->now.map, r);
}

} // namespace nnh
