// The particle batch's expand function (spec 024 FR-19, D39, plan "Expansion"): reads the emitter at submit, skips it
// whole when its bounds (grown by its largest quad) miss the view, then writes for each particle whose quad may touch
// the view exactly the item gfx::draw of its frame (or, without a texture, gfx::fill_rect of its square) would give,
// through the shared placement (gfx/sprite_place.hpp): camera, zoom (D33) and pixel rounding are one rule. Culling
// compares order keys of the positions against the view grown by the largest reach (conservative, no vmrs).
// Specialised per batch: <textured, local>.

#include <bit>

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

/// Frame `i` against a `tw` × `th` texture: its region (empty: the whole texture) and pivot; false outside the texture.
bool frame_geometry(const ParticleBlock& b, int i, int tw, int th, Region& r, Vec2& p) {
    const bool used = b.desc.frames[i].region.w >= 0;
    r = used ? b.desc.frames[i].region : Region{};
    if (r.empty()) r = {0, 0, tw, th};
    if (r.x + r.w > tw || r.y + r.h > th) return false;
    const bool centre = !used || b.desc.frames[i].center_pivot;
    p = centre ? Vec2{static_cast<float>(r.w) * 0.5f, static_cast<float>(r.h) * 0.5f} : b.desc.frames[i].pivot;
    return true;
}

/// How far a frame's quad reaches from its pivot: |dx| + |dy| of its farthest corner.
float frame_reach(const Region& r, Vec2 p) {
    const float dx = p.x > static_cast<float>(r.w) - p.x ? p.x : static_cast<float>(r.w) - p.x;
    const float dy = p.y > static_cast<float>(r.h) - p.y ? p.y : static_cast<float>(r.h) - p.y;
    const float ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    return ax + ay;
}

/// The frames' reach (1 for squares: |dx| + |dy| = side); false when a frame lies outside the texture. Remembered in
/// the block per texture size (T10.3): an emitter drawn with the same size again reads no frame.
bool frames_reach(ParticleBlock& b, const ParticleExpandRequest& q, float& reach) {
    reach = 1.0f;
    if (!q.textured) return true;
    if (b.reach_texture_w != q.texture_w || b.reach_texture_h != q.texture_h) {
        float largest = 0;
        bool fit = true;
        for (int i = 0; i < b.derived.frames && fit; ++i) {
            Region r;
            Vec2 p;
            fit = frame_geometry(b, i, q.texture_w, q.texture_h, r, p);
            const float a = fit ? frame_reach(r, p) : 0.0f;
            if (a > largest) largest = a;
        }
        b.reach_texture_w = q.texture_w;
        b.reach_texture_h = q.texture_h;
        b.frames_reach = largest;
        b.frames_fit = fit;
    }
    reach = b.frames_reach;
    return b.frames_fit;
}

/// The frames' regions and pivots for the expansion (they fit: frames_reach said so).
void resolve_frames(const ParticleBlock& b, const ParticleExpandRequest& q, ExpandSetup& s) {
    if (!q.textured) return;
    for (int i = 0; i < b.derived.frames; ++i) {
        (void)frame_geometry(b, i, q.texture_w, q.texture_h, s.regions[i], s.pivots[i]);
    }
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
#if defined(__GNUC__)
        // The walk is sequential (either way): fetch a few records ahead (the 3DS has no hardware prefetcher).
        const int ahead = d.newest_first ? i - 4 : i + 4;
        __builtin_prefetch(reinterpret_cast<const char*>(b.hot) + ahead * static_cast<int>(sizeof(ParticleHot)));
        __builtin_prefetch(reinterpret_cast<const char*>(b.cold) + ahead * static_cast<int>(sizeof(ParticleCold)));
#endif
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
        // Every field written one by one (no `Item{}`: GCC makes that a memset call per particle on the 3DS).
        Item& item = q.out[out.written++];
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
            item.flip_x = item.flip_y = false;
            item.texture = 0;
            item.region = {};
            item.pivot_x = item.pivot_y = 0;
            item.rotation = 0;
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
    float reach = 0;
    if (!frames_reach(b, q, reach)) {
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
    const float grown = reach * b.derived.largest_size * scale + 2.0f; // + rounding to whole pixels
    // zoom 1 (bits): the division would give the width itself
    const bool unzoomed = std::bit_cast<std::uint32_t>(q.zoom) == 0x3F800000u;
    const float view_w = unzoomed ? static_cast<float>(screen_width(q.screen))
                                  : static_cast<float>(screen_width(q.screen)) / q.zoom;
    const float view_h = unzoomed ? static_cast<float>(screen_height(q.screen))
                                  : static_cast<float>(screen_height(q.screen)) / q.zoom;
    const std::int32_t lo_x = order_key(q.camera.x - grown), lo_y = order_key(q.camera.y - grown);
    const std::int32_t hi_x = order_key(q.camera.x + view_w + grown), hi_y = order_key(q.camera.y + view_h + grown);
    if (q.first == 0) {
        const Rect r = world_bounds(b);
        if (order_key(r.x + r.w) < lo_x || order_key(r.x) > hi_x || order_key(r.y + r.h) < lo_y ||
            order_key(r.y) > hi_y) {
            out.culled = b.alive;
            b.culled += b.alive;
            return out;
        }
    }
    ExpandSetup s; // only for an emitter that may show
    resolve_frames(b, q, s);
    s.reach = grown;
    s.lo_x = lo_x;
    s.lo_y = lo_y;
    s.hi_x = hi_x;
    s.hi_y = hi_y;
    out = kExpands[(q.textured ? 2 : 0) + (local ? 1 : 0)](b, q, s);
    b.drawn += out.written;
    b.culled += out.culled;
    return out;
}

} // namespace nnh::detail
