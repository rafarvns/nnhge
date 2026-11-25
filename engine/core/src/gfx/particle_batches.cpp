// The frame's particle batches (see particle_batches.hpp): the pure table and budget, the gfx storage allocated at
// init() when AppConfig::gfx.max_particles > 0, and the ops gfx_frame.cpp calls through g_gfx_particle_ops. The
// registrar at the bottom sets that pointer before main(); this object is linked only by games that draw or destroy
// emitters, so every other game keeps a null pointer and pays nothing else.

#include "gfx/particle_batches.hpp"

#include "gfx/gfx_state.hpp"
#include "log/engine_channels.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/assert.hpp"
#include "nnh/log.hpp"

namespace nnh {

NNH_MEM_TAG_EXTERN(gfx);

namespace detail {

void ParticleBatches::reset(ParticleBatchRecord* records, int capacity, platform::DrawItem2D* scratch,
                            int scratch_capacity, int max_particles) {
    records_ = records;
    capacity_ = records != nullptr ? capacity : 0;
    scratch_ = scratch;
    scratch_capacity_ = scratch != nullptr ? scratch_capacity : 0;
    max_ = max_particles;
    clear();
}

void ParticleBatches::clear() {
    count_ = 0;
    drawn_ = 0;
    dropped_ = 0;
}

int ParticleBatches::add(const ParticleBatchRecord& record) {
    if (count_ >= capacity_) return -1;
    records_[count_] = record;
    return count_++;
}

const ParticleBatchRecord* ParticleBatches::get(int index) const {
    if (index < 0 || index >= count_ || records_[index].expand == nullptr) return nullptr;
    return &records_[index];
}

void ParticleBatches::forget(const void* source) {
    for (int i = 0; i < count_; ++i) {
        if (records_[i].source == source) records_[i].expand = nullptr;
    }
}

bool ParticleBatches::submit(int index, Screen screen, Camera2D camera, bool textured, platform::TextureHandle texture,
                             int tw, int th, std::uint32_t frame, SendFn send) {
    const ParticleBatchRecord* r = get(index);
    if (r == nullptr) return true;
    ParticleExpandRequest q;
    q.source = r->source;
    q.screen = screen;
    q.camera = r->ignore_camera ? Vec2{} : camera.position;
    q.zoom = r->ignore_camera ? 1.0f : camera.zoom;
    q.textured = textured;
    q.texture = texture;
    q.texture_w = tw;
    q.texture_h = th;
    q.tint = r->tint;
    q.layer = r->layer;
    q.frame = frame;
    q.out = scratch_;
    for (;;) {
        const int left = max_ - drawn_;
        q.capacity = left < scratch_capacity_ ? (left > 0 ? left : 0) : scratch_capacity_;
        const ParticleExpandResult result = r->expand(q);
        if (result.bad_frame) return false;
        drawn_ += result.written;
        dropped_ += result.dropped;
        if (result.written > 0) send(screen, scratch_, result.written);
        if (result.next < 0) return true;
        q.first = result.next;
    }
}

namespace {

struct ParticleGfx {
    ParticleBatches batches;
    ParticleBatchRecord* records = nullptr;
    platform::DrawItem2D* scratch = nullptr;
    int max_particles = 0;
    int last_frame = 0;
    int dropped_total = 0;
    std::uint32_t frame = 0;
    bool warned_off = false;
};
ParticleGfx g_particles;

void free_storage() {
    mem::free(g_particles.records);
    mem::free(g_particles.scratch);
    g_particles = ParticleGfx{};
}

bool ops_init(int max_particles) {
    free_storage();
    g_particles.max_particles = max_particles < 0 ? 0 : (max_particles > 65535 ? 65535 : max_particles);
    if (g_particles.max_particles == 0) return true;
    g_particles.records = static_cast<ParticleBatchRecord*>(
        mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(gfx), sizeof(ParticleBatchRecord) * kMaxParticleBatches,
                   alignof(ParticleBatchRecord)));
    g_particles.scratch = static_cast<platform::DrawItem2D*>(
        mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(gfx), sizeof(platform::DrawItem2D) * kParticleScratchItems,
                   alignof(platform::DrawItem2D)));
    if (g_particles.records == nullptr || g_particles.scratch == nullptr) return false;
    g_particles.batches.reset(g_particles.records, kMaxParticleBatches, g_particles.scratch, kParticleScratchItems,
                              g_particles.max_particles);
    return true;
}

void send_items(Screen screen, const platform::DrawItem2D* items, int count) { platform::draw_2d(screen, items, count); }

void ops_submit(Screen screen, int index, Camera2D camera) {
    const ParticleBatchRecord* r = g_particles.batches.get(index);
    if (r == nullptr) return; // its emitter was destroyed during the frame
    bool textured = false;
    platform::TextureHandle handle = 0;
    int tw = 0, th = 0;
    if (r->texture_slot >= 0) {
        const TextureData* t = gfx_state().textures.get(r->texture_slot, r->texture_generation);
        if (t == nullptr) {
            NNH_ASSERT(false, "particles drawn with a texture destroyed earlier");
            return;
        }
        textured = true;
        handle = t->platform;
        tw = t->width;
        th = t->height;
    }
    const bool ok = g_particles.batches.submit(index, screen, camera, textured, handle, tw, th, g_particles.frame,
                                               &send_items);
    NNH_ASSERT(ok, "a particle frame's region lies outside the %d x %d texture: the emitter was not drawn", tw, th);
}

void ops_end_frame() {
    ParticleGfx& p = g_particles;
    p.last_frame = p.batches.drawn();
    const int dropped = p.batches.dropped();
    p.batches.clear();
    ++p.frame;
    if (dropped > 0) {
        p.dropped_total += dropped;
        NNH_LOG_WARN(gfx, "%d particles dropped this frame (AppConfig::gfx.max_particles = %d)", dropped,
                     p.max_particles);
        NNH_ASSERT(false, "more than %d particles drawn in a frame: raise AppConfig::gfx.max_particles",
                   p.max_particles);
    }
}

void ops_stats(gfx::Stats& out) {
    out.particles_last_frame = g_particles.last_frame;
    out.max_particles = g_particles.max_particles;
    out.dropped_particles = g_particles.dropped_total;
}

int g_mark_drawn = 0, g_mark_dropped = 0; // budget_mark()

void ops_budget_mark() {
    g_mark_drawn = g_particles.batches.drawn();
    g_mark_dropped = g_particles.batches.dropped();
}

void ops_budget_rewind() { g_particles.batches.rewind(g_mark_drawn, g_mark_dropped); }

constexpr GfxParticleOps kOps{&ops_init,  &free_storage,    &ops_submit,       &ops_end_frame,
                              &ops_stats, &ops_budget_mark, &ops_budget_rewind};

/// Sets gfx's hook before main() (static initialisation of this object; D29's static nodes are the precedent).
struct Registrar {
    Registrar() { g_gfx_particle_ops = &kOps; }
};
const Registrar g_registrar;

} // namespace

void gfx_record_particles(Screen screen, int layer, const ParticleBatchRecord& record) {
    if (!gfx_drawing()) {
        NNH_ASSERT(false, "ParticleEmitter::draw outside a frame");
        return;
    }
    ParticleGfx& p = g_particles;
    if (p.max_particles == 0) {
        if (!p.warned_off) NNH_LOG_WARN(gfx, "particles not drawn: AppConfig::gfx.max_particles is 0");
        p.warned_off = true;
        NNH_ASSERT(false, "drawing particles needs AppConfig::gfx.max_particles > 0, e.g. .gfx = {.max_particles = 1024}");
        return;
    }
    ParticleBatchRecord stored = record;
    stored.layer = gfx_clamp_layer(layer);
    const int index = p.batches.add(stored);
    if (index < 0) {
        NNH_LOG_WARN(gfx, "more than %d particle batches in a frame: one dropped", kMaxParticleBatches);
        NNH_ASSERT(false, "more than %d particle draws in a frame", kMaxParticleBatches);
        return;
    }
    DrawCommand c;
    c.screen = screen;
    c.layer = stored.layer;
    c.tile_batch = static_cast<std::int16_t>(kParticleBatchBase + index);
    gfx_record(c);
}

void gfx_forget_particles(const void* source) {
    if (g_particles.max_particles > 0) g_particles.batches.forget(source);
}

} // namespace detail
} // namespace nnh
