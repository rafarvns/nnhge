// Notifications (spec 023 FR-16): on_covered after the stack change, on_finished in the last frame, the game's own
// finish event queued then, each exactly once; none on cancel; callbacks may query; a request from on_finished
// starts in the next frame.

#include "nnh/events.hpp"
#include "transition/transition_internal.hpp"
#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;

namespace {

struct DoorDone {
    int door;
};

struct Seen {
    int covered = 0, finished = 0, events = 0;
    std::uint32_t covered_frame = 0, finished_frame = 0, event_frame = 0;
    bool scene_changed_before_covered = false;
    bool running_in_finished = true;
    tr::Phase phase_in_covered = tr::Phase::None;
    int event_door = 0;
};
Seen g_seen;
Probe* g_b = nullptr;

tr::Options notifying(int out, int hold, int in) {
    return {.out = Frames{out},
            .hold = Frames{hold},
            .in = Frames{in},
            .on_covered =
                [] {
                    ++g_seen.covered;
                    g_seen.covered_frame = frame_no();
                    g_seen.scene_changed_before_covered = nnh::scenes::top() == g_b;
                    g_seen.phase_in_covered = tr::phase();
                },
            .on_finished =
                [] {
                    ++g_seen.finished;
                    g_seen.finished_frame = frame_no();
                    g_seen.running_in_finished = tr::running();
                },
            .publish = DoorDone{42}};
}

} // namespace

NNH_TEST(each_notification_comes_once_at_its_frame) {
    restart();
    g_seen = {};
    Probe a, b;
    g_b = &b;
    nnh::scenes::push(a);
    step();
    nnh::Subscription sub = nnh::events::subscribe<DoorDone>([](const DoorDone& e) {
        ++g_seen.events;
        g_seen.event_frame = frame_no();
        g_seen.event_door = e.door;
    });
    const std::uint32_t first = frame_no() + 1;
    CHECK(tr::replace(b, notifying(3, 2, 3)));
    CHECK(run_until_done() == 8);
    step();
    CHECK(g_seen.covered == 1 && g_seen.covered_frame == first + 3 && g_seen.scene_changed_before_covered);
    CHECK(g_seen.phase_in_covered == tr::Phase::Covered);
    CHECK(g_seen.finished == 1 && g_seen.finished_frame == first + 7 && !g_seen.running_in_finished);
    CHECK(g_seen.events == 1 && g_seen.event_frame == first + 8 && g_seen.event_door == 42); // delivered next frame
    nnh::shutdown();
}

NNH_TEST(a_cancelled_transition_notifies_nothing) {
    restart();
    g_seen = {};
    Probe a, b;
    g_b = &b;
    nnh::scenes::push(a);
    step();
    nnh::Subscription sub = nnh::events::subscribe<DoorDone>([](const DoorDone&) { ++g_seen.events; });
    CHECK(tr::replace(b, notifying(2, 0, 2)));
    step(3); // covered: on_covered ran
    nnh::shutdown();
    CHECK(g_seen.covered == 1 && g_seen.finished == 0 && g_seen.events == 0);
}

NNH_TEST(a_request_from_on_finished_starts_in_the_next_frame) {
    restart();
    static int second_started = 0;
    second_started = 0;
    CHECK(tr::cover({.out = Frames{1},
                     .in = Frames{1},
                     .on_finished = [] {
                         second_started = tr::cover({.out = Frames{2}, .in = Frames{2}}) ? 1 : 0;
                     }}));
    step(3);
    CHECK(second_started == 1 && tr::running() && tr::frames() == 0); // not stepped yet
    step();
    CHECK(tr::frames() == 1 && tr::phase() == tr::Phase::Out);
    run_until_done();
    nnh::shutdown();
}
