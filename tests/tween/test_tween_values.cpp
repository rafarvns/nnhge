// What a tween animates (spec 015 FR-5; T2.2): float, Vec2 and Color, by pointer and by setter; Color
// rounding and clamping on overshoot; the exact final write; custom easings.

#include "nnh/gfx.hpp"
#include "tween/tween_internal.hpp"
#include "tween_fixture.hpp"

using nnh::Color;
using nnh::Ease;
using nnh::Frames;
using nnh::TweenSet;
using nnh::Vec2;
using tween_test::run;

namespace {

struct Probe {
    Vec2 last{};
    int calls = 0;
};

float half_step(float t) {
    return t < 0.5f ? 0.0f : 1.0f;
}

} // namespace

NNH_TEST(values_pack_into_floats_per_kind) {
    float out[4] = {9, 9, 9, 9};
    const float f = 2.5f;
    nnh::detail::tween_pack(nnh::detail::TweenKind::Float, &f, out);
    CHECK(out[0] == 2.5f);
    const Vec2 v{3, -4};
    nnh::detail::tween_pack(nnh::detail::TweenKind::Vec2, &v, out);
    CHECK(out[0] == 3 && out[1] == -4);
    const Color c{1, 2, 3, 250};
    nnh::detail::tween_pack(nnh::detail::TweenKind::Color, &c, out);
    CHECK(out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == 250);
    CHECK(nnh::detail::tween_components(nnh::detail::TweenKind::Float) == 1);
    CHECK(nnh::detail::tween_components(nnh::detail::TweenKind::Vec2) == 2);
    CHECK(nnh::detail::tween_components(nnh::detail::TweenKind::Color) == 4);
}

NNH_TEST(a_float_tween_by_pointer_interpolates_linearly) {
    TweenSet set({.tweens = 4});
    float x = 0;
    set.to(&x, 60.0f, Frames{60});
    run(set, 1);
    CHECK(x == 1.0f);
    run(set, 29);
    CHECK(x == 30.0f);
    run(set, 30);
    CHECK(x == 60.0f);
}

NNH_TEST(a_vec2_tween_interpolates_each_component) {
    TweenSet set({.tweens = 4});
    Vec2 p{10, 100};
    set.to(&p, {20, 0}, Frames{4});
    run(set, 2);
    CHECK(p == (Vec2{15, 50}));
}

NNH_TEST(a_color_tween_rounds_each_channel_to_nearest) {
    TweenSet set({.tweens = 4});
    Color c{0, 0, 0, 0};
    set.to(&c, Color{3, 255, 1, 10}, Frames{4});
    run(set, 1); // a quarter: 0.75, 63.75, 0.25, 2.5
    CHECK(c.r == 1 && c.g == 64 && c.b == 0 && c.a == 3);
}

NNH_TEST(a_color_tween_clamps_overshoot_instead_of_wrapping) {
    TweenSet set({.tweens = 4});
    Color c{0, 0, 0, 255};
    set.to(&c, Color{255, 0, 200, 255}, Frames{100}, {.ease = Ease::BackOut});
    bool overshoot_seen = false;
    for (int i = 0; i < 99; ++i) {
        set.update();
        CHECK(c.r <= 255 && c.g == 0);
        if (c.r == 255) overshoot_seen = true;
    }
    CHECK(overshoot_seen);
    Color d{255, 255, 255, 255};
    set.to(&d, Color{0, 0, 0, 0}, Frames{100}, {.ease = Ease::BackIn});
    run(set, 10); // BackIn starts below 0: the colour goes up past 255, clamped
    CHECK(d.r == 255 && d.a == 255);
}

NNH_TEST(the_final_value_is_the_end_value_exactly) {
    TweenSet set({.tweens = 4});
    float x = 0.1f;
    set.to(&x, 0.7f, Frames{3}, {.ease = Ease::ElasticOut});
    run(set, 3);
    CHECK(x == 0.7f);
}

NNH_TEST(a_setter_target_receives_every_value) {
    TweenSet set({.tweens = 4});
    Probe probe;
    set.from_to(nnh::Setter<Vec2>{[&probe](Vec2 v) {
                    probe.last = v;
                    ++probe.calls;
                }},
                {0, 0}, {8, 4}, Frames{4});
    CHECK(probe.calls == 1); // `from` is written at once
    CHECK(probe.last == (Vec2{0, 0}));
    run(set, 2);
    CHECK(probe.calls == 3);
    CHECK(probe.last == (Vec2{4, 2}));
    run(set, 2);
    CHECK(probe.last == (Vec2{8, 4}));
    run(set, 2);
    CHECK(probe.calls == 5); // finished: no more writes
}

NNH_TEST(setters_work_for_float_and_color_too) {
    TweenSet set({.tweens = 4});
    float f = 0;
    Color c{};
    set.from_to(nnh::Setter<float>{[&f](float v) { f = v; }}, 0.0f, 2.0f, Frames{2});
    set.from_to(nnh::Setter<Color>{[&c](Color v) { c = v; }}, Color{0, 0, 0, 0}, Color{100, 50, 0, 255},
                Frames{2});
    run(set, 1);
    CHECK(f == 1.0f);
    CHECK(c.r == 50 && c.g == 25 && c.b == 0 && c.a == 128);
}

NNH_TEST(a_custom_easing_function_shapes_the_tween) {
    TweenSet set({.tweens = 4});
    float x = 0;
    set.to(&x, 10.0f, Frames{4}, {.ease = half_step});
    run(set, 1);
    CHECK(x == 0.0f);
    run(set, 1);
    CHECK(x == 10.0f);
}
