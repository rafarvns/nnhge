// The effect geometry as public pure queries and a plain draw call (spec 023 FR-9): what the transitions draw,
// usable with no transition, no scene stack and (except draw_effect) no init.

#include "nnh/transition.hpp"
#include "transition/effect_geometry.hpp"

namespace nnh {
namespace {

struct ArraySink {
    Rect* out;
    int capacity;
    int count;
};

void to_array(void* ctx, const detail::PixelRect& r) {
    auto* sink = static_cast<ArraySink*>(ctx);
    if (sink->count < sink->capacity) {
        sink->out[sink->count] = Rect{static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w),
                                      static_cast<float>(r.h)};
    }
    ++sink->count;
}

struct DrawSink {
    Screen screen;
    Color color;
    ShapeOptions options;
};

void to_gfx(void* ctx, const detail::PixelRect& r) {
    const auto* sink = static_cast<const DrawSink*>(ctx);
    gfx::fill_rect(sink->screen,
                   Rect{static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w),
                        static_cast<float>(r.h)},
                   sink->color, sink->options);
}

detail::EffectArea area_of(const transition::Placement& p) {
    return detail::EffectArea{p.tall, p.tall_gap < 0 ? 0 : (p.tall_gap > 240 ? 240 : p.tall_gap)};
}

} // namespace

void detail::draw_resolved(const ResolvedEffect& fx, Screen screen, EffectArea area, int q, int layer) {
    DrawSink sink{screen, effect_color_q(fx, q), ShapeOptions{.layer = layer, .ignore_camera = true}};
    draw_screen_effect(fx, screen, area, q, &to_gfx, &sink);
}

int transition::max_rects(const Effect& effect, bool tall) {
    return detail::max_rects_resolved(detail::resolve_effect(effect, Screen::Top, {}, false), tall);
}

int transition::effect_rects(Screen screen, const Effect& effect, float coverage, Rect* out, int capacity,
                             const Placement& placement) {
    const detail::EffectArea area = area_of(placement);
    const detail::ResolvedEffect fx = detail::resolve_effect(effect, screen, area, placement.continued);
    ArraySink sink{out, out != nullptr && capacity > 0 ? capacity : 0, 0};
    detail::draw_screen_effect(fx, screen, area, detail::coverage_q(coverage), &to_array, &sink);
    return sink.count;
}

Color transition::effect_color(const Effect& effect, float coverage) {
    return detail::effect_color_q(detail::resolve_effect(effect, Screen::Top, {}, false),
                                  detail::coverage_q(coverage));
}

void transition::draw_effect(Screen screen, const Effect& effect, float coverage, const Placement& placement) {
    const detail::EffectArea area = area_of(placement);
    detail::draw_resolved(detail::resolve_effect(effect, screen, area, placement.continued), screen, area,
                          detail::coverage_q(coverage), placement.layer);
}

} // namespace nnh
