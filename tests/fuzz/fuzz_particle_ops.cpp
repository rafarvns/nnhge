// Fuzz target body for particles (spec 024 T5.5), see fuzz_particle_ops.hpp.

#include "fuzz_particle_ops.hpp"

#include <bit>
#include <cstdlib>
#include <cstring>

#include "gfx/particle_batches.hpp"
#include "nnh/particles.hpp"
#include "particles/particle_block.hpp"

namespace nnh::detail {
ParticleExpandResult particle_expand(const ParticleExpandRequest& q); // particle_expand.cpp
}

namespace nnh::fuzz {
namespace {

struct Reader {
    const std::uint8_t* data;
    std::size_t size;
    std::size_t at = 0;
    std::uint32_t byte() { return at < size ? data[at++] : 0u; }
    std::uint32_t word() { return byte() | (byte() << 8) | (byte() << 16) | (byte() << 24); }
    bool more() const { return at < size; }
    /// Any finite float (non-finite bits become 0), or a small one half the time.
    float value() {
        if (byte() & 1u) return static_cast<float>(static_cast<int>(byte()) - 128) * 0.5f;
        const float f = std::bit_cast<float>(word());
        return (std::bit_cast<std::uint32_t>(f) & 0x7F800000u) == 0x7F800000u ? 0.0f : f;
    }
};

void require(bool ok) {
    if (!ok) std::abort();
}

ParticleDesc read_desc(Reader& r) {
    ParticleDesc d;
    if (r.byte() & 1u) { // raw bytes over the whole description: mostly invalid
        auto* p = reinterpret_cast<unsigned char*>(&d);
        for (std::size_t i = 0; i < sizeof d && r.more(); ++i) p[i] = static_cast<unsigned char>(r.byte());
        // Bools must hold 0 or 1 (any other byte is undefined behaviour to read, not a description to refuse).
        auto bit = [](bool& b) { b = (std::bit_cast<unsigned char>(b) & 1u) != 0; };
        bit(d.looping);
        bit(d.edge);
        bit(d.world_gravity);
        for (ParticleFrame& f : d.frames) bit(f.center_pivot);
        return d;
    }
    d.rate = static_cast<float>(r.byte() * 4);
    d.shape = static_cast<ParticleShape>(r.byte() % 4);
    d.box = {static_cast<float>(r.byte()), static_cast<float>(r.byte())};
    d.edge = (r.byte() & 1u) != 0;
    d.radius = static_cast<float>(r.byte());
    d.inner_radius = d.radius * static_cast<float>(r.byte()) / 255.0f;
    d.length = static_cast<float>(r.byte());
    d.direction = static_cast<ParticleDirection>(r.byte() % 2);
    d.spread = kFullTurn * static_cast<float>(r.byte()) / 255.0f;
    d.speed = {r.value(), r.value()};
    d.gravity = {r.value(), r.value()};
    d.damping = static_cast<float>(r.byte() % 61);
    d.spin = {r.value(), r.value()};
    d.lifetime = {FrameTime::from_ticks(1 + r.byte() * 300), FrameTime::from_ticks(1 + r.byte() * 600)};
    if (d.lifetime.min.ticks > d.lifetime.max.ticks) d.lifetime.max = d.lifetime.min;
    d.bursts[0] = {.at = Frames{static_cast<int>(r.byte() % 30)},
                   .count = {static_cast<std::uint16_t>(r.byte() % 40), static_cast<std::uint16_t>(40 + r.byte())},
                   .cycles = static_cast<std::uint16_t>(r.byte() % 5),
                   .interval = Frames{1 + static_cast<int>(r.byte() % 20)}};
    d.space = static_cast<ParticleSpace>(r.byte() % 2);
    d.world_gravity = (r.byte() & 1u) != 0;
    d.color_keys[0] = {0, colors::white};
    d.color_keys[1] = {static_cast<float>(r.byte()) / 255.0f, rgba(1, 2, 3, 4)};
    d.size = {0, static_cast<float>(r.byte() % 8)};
    d.frame_mode = static_cast<ParticleFrameMode>(r.byte() % 4);
    for (int i = 0; i < static_cast<int>(r.byte() % 5); ++i) d.frames[i].region = {i * 4, 0, 4, 4};
    d.frame_rate = static_cast<float>(r.byte() % 200);
    d.prewarm = static_cast<std::uint16_t>(r.byte() % 30);
    d.seed = r.word();
    return d;
}

void check(const ParticleEmitter& e) {
    require(e.alive() <= e.capacity());
    const Rect b = e.bounds();
    if (e.desc().space != ParticleSpace::World) return;
    for (int i = 0; i < e.alive(); ++i) {
        const Vec2 p = e.particle(i).position;
        const bool finite = (std::bit_cast<std::uint32_t>(p.x) & 0x7F800000u) != 0x7F800000u &&
                            (std::bit_cast<std::uint32_t>(p.y) & 0x7F800000u) != 0x7F800000u;
        if (finite && (b.w == b.w && b.h == b.h)) require(p.x >= b.x && p.y >= b.y);
    }
}

void expand(const ParticleEmitter& e, Reader& r) {
    static platform::DrawItem2D scratch[32];
    detail::ParticleExpandRequest q;
    q.source = ParticleAccess::block(e);
    q.camera = {r.value(), r.value()};
    q.zoom = 0.5f + static_cast<float>(r.byte() % 8) * 0.5f;
    q.textured = (r.byte() & 1u) != 0;
    q.texture_w = 64;
    q.texture_h = 32;
    q.out = scratch;
    q.capacity = static_cast<int>(r.byte() % 33);
    q.frame = r.byte();
    for (int guard = 0; guard < 400; ++guard) {
        const detail::ParticleExpandResult res = detail::particle_expand(q);
        require(res.written <= q.capacity && res.written >= 0);
        if (res.next < 0 || res.bad_frame) return;
        require(res.next > q.first || q.first == 0);
        q.first = res.next;
    }
}

} // namespace

int particle_ops_target(const std::uint8_t* data, std::size_t size) {
    Reader r{data, size};
    const int capacity = 1 + static_cast<int>(r.byte() % 200);
    auto created = ParticleEmitter::create(read_desc(r), capacity);
    if (!created) return 0;
    ParticleEmitter e = static_cast<ParticleEmitter&&>(created.value());
    while (r.more()) {
        switch (r.byte() % 11) {
        case 0: e.update(); break;
        case 1: e.emit(static_cast<int>(r.byte())); break;
        case 2: e.emit_at(Vec2{r.value(), r.value()}, static_cast<int>(r.byte() % 50)); break;
        case 3: {
            const Transform2D t{{r.value(), r.value()}, r.value(), {r.value(), r.value()}};
            e.set_transform(t, (r.byte() & 1u) != 0);
            break;
        }
        case 4: e.set_position({r.value(), r.value()}); break;
        case 5: {
            const ParticleDesc d = read_desc(r);
            if (ParticleEmitter::check(d)) e.set_desc(d);
            break;
        }
        case 6: e.restart(); break;
        case 7: e.stop(); break;
        case 8: e.start(); break;
        case 9: e.clear(); break;
        default: expand(e, r); break;
        }
        check(e);
    }
    return 0;
}

} // namespace nnh::fuzz
