// Drawing an emitter (spec 024 FR-19, D39): one particle batch recorded into gfx's table and one draw-list command; the
// particles are read and expanded at submit (particle_expand.cpp). A destroyed emitter's batches are forgotten.

#include "gfx/gfx_state.hpp"
#include "nnh/texture.hpp"
#include "particles/particle_appearance.hpp"

namespace nnh {
namespace detail {

ParticleExpandResult particle_expand(const ParticleExpandRequest& q); // particle_expand.cpp

namespace {

void record(const ParticleBlock* b, Screen screen, int slot, std::uint32_t generation, const ParticleDrawOptions& o) {
    ParticleBatchRecord r;
    r.expand = &particle_expand;
    r.source = b;
    r.texture_slot = slot;
    r.texture_generation = generation;
    const float opacity = o.opacity < 0 ? 0 : (o.opacity > 1 ? 1 : o.opacity);
    Color tint = o.tint;
    tint.a = static_cast<std::uint8_t>(static_cast<float>(o.tint.a) * opacity + 0.5f); // as gfx::draw
    r.tint = pack_color(tint);
    r.ignore_camera = o.ignore_camera;
    gfx_record_particles(screen, o.layer, r);
}

} // namespace
} // namespace detail

void ParticleEmitter::draw(Screen screen, const ParticleDrawOptions& options) const {
    if (block_ == nullptr) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::draw on an empty emitter");
        return;
    }
    detail::record(block_, screen, -1, 0, options);
}

void ParticleEmitter::draw(Screen screen, const Texture& texture, const ParticleDrawOptions& options) const {
    if (block_ == nullptr || !texture.valid()) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::draw: an empty emitter or an empty texture");
        return;
    }
    detail::record(block_, screen, TextureAccess::slot(texture), TextureAccess::generation(texture), options);
}

} // namespace nnh
