// The particle batch's expand function (spec 024 FR-19, D39, plan "Expansion"): reads the emitter at submit, skips it
// whole when its bounds (grown by its largest quad) miss the view, then writes for each particle whose quad may touch
// the view exactly the item gfx::draw of its frame (or, without a texture, gfx::fill_rect of its square) would give,
// through the shared placement (gfx/sprite_place.hpp): camera, zoom (D33) and pixel rounding are one rule. Culling
// compares order keys of the positions against the view grown by the largest reach (conservative, no vmrs).
// Specialised per batch: <textured, local>.

#include "gfx/particle_batches.hpp"
#include "gfx/sprite_place.hpp"
#include "particles/particle_appearance.hpp"
#include "particles/particle_bits.hpp"
#include "particles/particle_map.hpp"

namespace nnh::detail {
namespace {

using Item = platform::DrawItem2D;

/// The batch's constants, computed once per expand call.
struct ExpandSetup {
    Region regions[kMaxParticleFrames];
    Vec2 pivots[kMaxParticleFrames];
    std::int32_t lo_x, lo_y, hi_x, hi_y; ///< order keys of the view grown by the reach
    float reach = 0;                     ///< world pixels around a particle's position its quad can cover
};

/// Resolves the frames against the texture; false when one lies outside it.
bool resolve_frames(const ParticleBlock& b, const ParticleExpandRequest& q, ExpandSetup& s, float& reach) {
    reach = 1.0f; // a square: |dx| + |dy| = side
    if (!q.textured) return true;
    reach = 0;
    for (int i = 0; i < b.derived.frames; ++i) {
        const bool used = b.desc.frames[i].region.w >= 0;
        Region r = used ? b.desc.frames[i].region : Region{};
        if (r.empty()) r = {0, 0, q.texture_w, q.texture_h};
        if (r.x + r.w > q.texture_w || r.y + r.h > q.texture_h) return false;
        const bool centre = !used || b.desc.frames[i].center_pivot;
        const Vec2 p = centre ? Vec2{static_cast<float>(r.w) * 0.5f, static_cast<float>(r.h) * 0.5f}
                              : b.desc.frames[i].pivot;
        s.regions[i] = r;
        s.pivots[i] = p;
        const float dx = p.x > static_cast<float>(r.w) - p.x ? p.x : static_cast<float>(r.w) - p.x;
        const float dy = p.y > static_cast<float>(r.h) - p.y ? p.y : static_cast<float>(r.h) - p.y;
        const float ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        if (ax + ay > reach) reach = ax + ay;
    }
    return true;
}

/// The emitter's world bounds (local space: through its map).
Rect world_bounds(const ParticleBlock& b) {
    const float x0 = key_float(b.key_min_x), y0 = key_float(b.key_min_y);
    const Rect r{x0, y0, key_float(b.key_max_x) - x0, key_float(b.key_max_y) - y0};
    return b.derived.world ? r : bounds(b.now.map, r);
}

template <bool Textured, bool Local>
ParticleExpandResult expand_particles(ParticleBlock& b, const ParticleExpandRequest& q, const ExpandSetup& s) {
    ParticleExpandResult out;
    const ParticleDerived& d = b.derived;
    const Affine2D& map = b.now.map;
    const std::uint32_t mirror = mirror_bit(b.now.scale);
    const int n = b.alive;
    for (int k = q.first; k < n; ++k) {
        const int i = d.newest_first ? n - 1 - k : k;
        const ParticleHot& h = b.hot[i];
        const Vec2 p = Local ? map_point(map, {h.px, h.py}) : Vec2{h.px, h.py};
        const std::int32_t kx = order_key(p.x), ky = order_key(p.y);
        if (kx < s.lo_x || kx > s.hi_x || ky < s.lo_y || ky > s.hi_y) {
            ++out.culled;
            continue;
        }
        if (q.capacity == 0) {
            ++out.dropped;
            continue;
        }
        if (out.written == q.capacity) {
            out.next = k;
            return out;
        }
        const ParticleCold& c = b.cold[i];
        const float t = life_fraction(h, c);
        const float size = size_at(d.size, t, c.size);
        Item& item = q.out[out.written++];
        item = Item{};
        item.layer = q.layer;
        item.color = unpack_color(mul_color(color_at(d.color, t, c.color), q.tint));
        const Vec2 at{p.x - q.camera.x, p.y - q.camera.y};
        if constexpr (Textured) {
            const int f = frame_at(d, h, c, t);
            item.kind = Item::Kind::Sprite;
            item.texture = q.texture;
            const float rotation = Local ? b.now.rotation + flip_sign(h.rotation, mirror) : h.rotation;
            const Vec2 scale = Local ? Vec2{b.now.scale.x * size, b.now.scale.y * size} : Vec2{size, size};
            place_sprite(s.regions[f], false, false, scale, s.pivots[f], rotation, at, q.zoom, item);
        } else {
            item.kind = Item::Kind::FillRect;
            const float sx = Local ? (b.now.scale.x < 0 ? -b.now.scale.x : b.now.scale.x) * size : size;
            const float sy = Local ? (b.now.scale.y < 0 ? -b.now.scale.y : b.now.scale.y) * size : size;
            const Vec2 corner{(p.x - sx * 0.5f) - q.camera.x, (p.y - sy * 0.5f) - q.camera.y};
            place_rect(corner, {sx, sy}, q.zoom, item);
        }
    }
    return out;
}

using ExpandFn = ParticleExpandResult (*)(ParticleBlock&, const ParticleExpandRequest&, const ExpandSetup&);
constexpr ExpandFn kExpands[4] = {&expand_particles<false, false>, &expand_particles<false, true>,
                                  &expand_particles<true, false>, &expand_particles<true, true>};

} // namespace

ParticleExpandResult particle_expand(const ParticleExpandRequest& q) {
    auto& b = *const_cast<ParticleBlock*>(static_cast<const ParticleBlock*>(q.source));
    if (b.stats_frame != q.frame) {
        b.stats_frame = q.frame;
        b.drawn = 0;
        b.culled = 0;
    }
    ParticleExpandResult out;
    if (b.alive == 0) return out;
    ExpandSetup s;
    float reach = 0;
    if (!resolve_frames(b, q, s, reach)) {
        out.bad_frame = true;
        return out;
    }
    const bool local = !b.derived.world;
    float scale = 1;
    if (local) {
        const float ax = b.now.scale.x < 0 ? -b.now.scale.x : b.now.scale.x;
        const float ay = b.now.scale.y < 0 ? -b.now.scale.y : b.now.scale.y;
        scale = ax > ay ? ax : ay;
    }
    s.reach = reach * b.derived.largest_size * scale + 2.0f; // + rounding to whole pixels
    const float view_w = static_cast<float>(screen_width(q.screen)) / q.zoom;
    const float view_h = static_cast<float>(screen_height(q.screen)) / q.zoom;
    s.lo_x = order_key(q.camera.x - s.reach);
    s.lo_y = order_key(q.camera.y - s.reach);
    s.hi_x = order_key(q.camera.x + view_w + s.reach);
    s.hi_y = order_key(q.camera.y + view_h + s.reach);
    if (q.first == 0) {
        const Rect r = world_bounds(b);
        if (order_key(r.x + r.w) < s.lo_x || order_key(r.x) > s.hi_x || order_key(r.y + r.h) < s.lo_y ||
            order_key(r.y) > s.hi_y) {
            out.culled = b.alive;
            b.culled += b.alive;
            return out;
        }
    }
    out = kExpands[(q.textured ? 2 : 0) + (local ? 1 : 0)](b, q, s);
    b.drawn += out.written;
    b.culled += out.culled;
    return out;
}

} // namespace nnh::detail
