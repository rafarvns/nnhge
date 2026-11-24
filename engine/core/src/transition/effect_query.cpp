// The effect geometry as public pure queries and a plain draw call (spec 023 FR-9): what the transitions draw,
// usable with no transition, no scene stack and (except draw_effect) no init.

#include "gfx/gfx_state.hpp"
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

/// Records one rectangle: the command gfx::fill_rect builds, filled once per screen (`ctx`), only its place set here.
void to_draw_list(void* ctx, const detail::PixelRect& r) {
    auto* command = static_cast<detail::DrawCommand*>(ctx);
    command->position = Vec2{static_cast<float>(r.x), static_cast<float>(r.y)};
    command->second = Vec2{static_cast<float>(r.w), static_cast<float>(r.h)};
    detail::gfx_record(*command);
}

detail::EffectArea area_of(const transition::Placement& p) {
    return detail::EffectArea{p.tall, p.tall_gap < 0 ? 0 : (p.tall_gap > 240 ? 240 : p.tall_gap)};
}

} // namespace

// What gfx::fill_rect(rect, colour, {.layer, .ignore_camera = true}) records for each rectangle, bit for bit, with its
// checks and command built once per screen instead of once per rectangle (plan 023 T10.3; pinned by
// tests/transition/test_effect_query.cpp).
void detail::draw_resolved(const ResolvedEffect& fx, Screen screen, EffectArea area, int q, int layer) {
    if (!gfx_drawing()) return; // outside a frame: nothing (the geometry has no other effect)
    DrawCommand command;
    command.kind = platform::DrawItem2D::Kind::FillRect;
    command.screen = screen;
    command.layer = gfx_clamp_layer(layer);
    command.ignore_camera = true;
    command.color = effect_color_q(fx, q);
    draw_screen_effect(fx, screen, area, q, &to_draw_list, &command);
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
