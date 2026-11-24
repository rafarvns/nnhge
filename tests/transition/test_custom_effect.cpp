// Custom effects (spec 023 FR-8): called once per frame for each screen whose effect is Custom, with the screen,
// coverage, phase, frame of the phase, loading progress and tall flag, every transition frame (coverage 0 too);
// nothing built-in is drawn for that screen; a lambda with 16 bytes of captures fits.

#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;
using nnh::Screen;
using tr::Phase;

namespace {

struct Calls {
    static constexpr int kMax = 64;
    tr::CustomFrame frames[kMax];
    int n = 0;
    std::uint32_t frame_of[kMax] = {};
};

} // namespace

NNH_TEST(a_custom_effect_is_called_per_custom_screen_every_frame) {
    restart();
    Calls calls;
    int extra = 7; // a second capture: 16 bytes with the pointer on 64-bit PCs
    int* extra_ptr = &extra;
    CHECK(tr::cover({.effect = tr::custom(),
                     .bottom = tr::fade(),
                     .out = Frames{2},
                     .hold = Frames{1},
                     .in = Frames{2},
                     .custom = [&calls, extra_ptr](const tr::CustomFrame& f) {
                         if (calls.n < Calls::kMax) {
                             calls.frames[calls.n] = f;
                             calls.frame_of[calls.n] = frame_no() + static_cast<std::uint32_t>(*extra_ptr - 7);
                         }
                         ++calls.n;
                     }}));
    CHECK(run_until_done() == 5);
    CHECK(calls.n == 5); // the top screen only, every frame
    const Phase phases[] = {Phase::Out, Phase::Out, Phase::Covered, Phase::In, Phase::In};
    const float coverage[] = {0.5f, 1.0f, 1.0f, 0.5f, 0.0f};
    const int frames[] = {1, 2, 1, 1, 2};
    bool ok = true;
    for (int i = 0; i < 5; ++i) {
        const tr::CustomFrame& f = calls.frames[i];
        ok = ok && f.screen == Screen::Top && f.phase == phases[i] && f.coverage == coverage[i] &&
             f.frame == frames[i] && f.loading == 1.0f && !f.tall;
        ok = ok && (i == 0 || calls.frame_of[i] == calls.frame_of[i - 1] + 1);
    }
    CHECK(ok);
    nnh::shutdown();
}

NNH_TEST(a_custom_screen_gets_no_built_in_drawing) {
    restart();
    CHECK(tr::cover({.effect = tr::custom(), .out = Frames{1}, .custom = [](const tr::CustomFrame&) {}}));
    step();
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 0);
    nnh::shutdown();
}

NNH_TEST(a_custom_reveal_after_a_built_in_cover) {
    restart();
    int calls = 0;
    bool tall = false;
    CHECK(tr::cover({.effect = tr::wipe(tr::Edge::Top),
                     .reveal = tr::custom(),
                     .tall = true,
                     .out = Frames{1},
                     .in = Frames{3},
                     .custom = [&calls, &tall](const tr::CustomFrame& f) {
                         ++calls;
                         tall = f.tall && f.phase == Phase::In;
                     }}));
    CHECK(run_until_done() == 5);
    CHECK(calls == 6 && tall); // both screens, three in frames
    nnh::shutdown();
}
