// Plain loops (spec 023 FR-18, FR-3): a hand-written loop with scenes::update_and_draw() gives the frames of
// nnh::run(); cover() works with no scene at all; a long frame skips no transition frame.

#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;

namespace {

struct Trace {
    char text[256] = {};
    int n = 0;
    void add(char c) {
        if (n < 255) text[n++] = c;
    }
};
Trace g_trace;
Probe* g_next = nullptr;

/// Records each frame as the phase's initial (o, c, i, -) and requests a replace on its third update.
void script(Probe& self) {
    if (self.updates == 3) CHECK(tr::replace(*g_next, {.out = Frames{2}, .hold = Frames{1}, .in = Frames{2}}));
}

class Recorder : public Probe {
public:
    void draw() override {
        Probe::draw();
        const tr::Phase p = tr::phase();
        g_trace.add(p == tr::Phase::Out ? 'o' : p == tr::Phase::Covered ? 'c' : p == tr::Phase::In ? 'i' : '-');
    }
};

/// Pops itself on its third update after the transition.
void end_after(Probe& self) {
    if (self.updates == 3) nnh::scenes::pop();
}

} // namespace

NNH_TEST(a_manual_loop_gives_the_frames_of_run) {
    Trace with_run;
    for (int pass = 0; pass < 2; ++pass) {
        restart();
        g_trace = {};
        Recorder a, b;
        g_next = &b;
        a.on_update = script;
        b.on_update = end_after;
        if (pass == 0) {
            nnh::run(a);
            with_run = g_trace;
        } else {
            nnh::scenes::push(a);
            while (nnh::frame() && (nnh::scenes::size() > 0 || tr::running())) nnh::scenes::update_and_draw();
        }
        nnh::shutdown();
    }
    // The request's own frame already reports Out (running from the request); two out, one covered, two in.
    CHECK(std::strcmp(with_run.text, "--ooocii---") == 0 && std::strcmp(g_trace.text, with_run.text) == 0);
}

NNH_TEST(cover_works_with_no_scene_at_all) {
    restart();
    CHECK(tr::cover({.out = Frames{2}, .in = Frames{2}}));
    CHECK(nnh::frame());
    nnh::scenes::update_and_draw(); // the per-frame step of a game without scenes
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 2); // the fade on both screens
    CHECK(run_until_done() == 4);
    nnh::shutdown();
}

NNH_TEST(a_long_frame_skips_no_transition_frame) {
    restart();
    CHECK(tr::cover({.out = nnh::Duration::from_ms(500), .in = nnh::Duration::from_ms(500)}));
    int frames = 0;
    while (tr::running() && frames < 1000) {
        fake::advance_time(nnh::Duration::from_seconds(frames == 3 ? 5 : 0)); // a debugger stop
        step();
        ++frames;
    }
    CHECK(frames == 30 + 1 + 30);
    nnh::shutdown();
}
