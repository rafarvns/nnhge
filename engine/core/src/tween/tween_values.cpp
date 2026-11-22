// Values of a tween (spec 015 FR-5): packed as floats, interpolated per component, written by pointer or
// setter. Color channels are clamped to 0–255 and rounded to nearest, so overshooting curves never wrap a
// colour.

#include "nnh/gfx.hpp"
#include "tween/tween_internal.hpp"

namespace nnh::detail {
namespace {

std::uint8_t to_channel(float v) {
    if (!(v > 0.0f)) return 0; // also NaN
    if (v >= 255.0f) return 255;
    return static_cast<std::uint8_t>(static_cast<int>(v + 0.5f));
}

/// The progress of the current loop, mirrored when a yoyo loop plays backwards.
float slot_progress(const SlotHot& s) {
    if (s.duration <= 0) return 1.0f;
    const std::int32_t t = (s.flags & slot_flag::kBackwards) != 0 ? s.duration - s.elapsed : s.elapsed;
    return static_cast<float>(t) / static_cast<float>(s.duration);
}

} // namespace

int tween_components(TweenKind kind) {
    switch (kind) {
    case TweenKind::Float:
        return 1;
    case TweenKind::Vec2:
        return 2;
    case TweenKind::Color:
        return 4;
    case TweenKind::Timer:
    case TweenKind::Sequence:
        break;
    }
    return 0;
}

void tween_pack(TweenKind kind, const void* value, float out[4]) {
    switch (kind) {
    case TweenKind::Float:
        out[0] = *static_cast<const float*>(value);
        break;
    case TweenKind::Vec2: {
        const Vec2& v = *static_cast<const Vec2*>(value);
        out[0] = v.x;
        out[1] = v.y;
        break;
    }
    case TweenKind::Color: {
        const Color& c = *static_cast<const Color*>(value);
        out[0] = c.r;
        out[1] = c.g;
        out[2] = c.b;
        out[3] = c.a;
        break;
    }
    case TweenKind::Timer:
    case TweenKind::Sequence:
        break;
    }
}

void tween_write(const SlotHot& s, const float v[4]) {
    float f = v[0];
    Vec2 p{v[0], v[1]};
    Color c{to_channel(v[0]), to_channel(v[1]), to_channel(v[2]), to_channel(v[3])};
    switch (s.kind) {
    case TweenKind::Float:
        if (s.target != nullptr) *static_cast<float*>(s.target) = f;
        else if (s.setter) s.setter(&f);
        break;
    case TweenKind::Vec2:
        if (s.target != nullptr) *static_cast<Vec2*>(s.target) = p;
        else if (s.setter) s.setter(&p);
        break;
    case TweenKind::Color:
        if (s.target != nullptr) *static_cast<Color*>(s.target) = c;
        else if (s.setter) s.setter(&c);
        break;
    case TweenKind::Timer:
    case TweenKind::Sequence:
        break;
    }
}

void tween_write_current(const SlotHot& s) {
    if (tween_components(s.kind) == 0) return; // timers and sequences have no value
    const float p = slot_progress(s);
    const float e = s.custom != nullptr ? s.custom(p) : ease(s.ease, p);
    float v[4] = {};
    const int n = tween_components(s.kind);
    for (int i = 0; i < n; ++i) {
        v[i] = s.from[i] + (s.to[i] - s.from[i]) * e;
    }
    tween_write(s, v);
}

void tween_begin(SlotHot& s) {
    s.flags |= slot_flag::kStarted;
    if ((s.flags & slot_flag::kHasFrom) != 0 || s.target == nullptr) return;
    tween_pack(s.kind, s.target, s.from);
    if ((s.flags & slot_flag::kRelative) != 0) {
        for (int i = 0; i < tween_components(s.kind); ++i) {
            s.to[i] += s.from[i];
        }
    }
}

} // namespace nnh::detail
