// nnh::run() and the scene stack inside the frame loop (FR-8, FR-10).

#include "nnh/app.hpp"
#include "nnh/events.hpp"
#include "nnh/input.hpp"
#include "nnh/platform_fake.hpp"
#include "recording_scene.hpp"
#include "test.hpp"

using scene_test::Log;
using scene_test::RecordingScene;

namespace fake = nnh::platform_fake;

namespace {

void restart() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
}

/// Pops itself after `frames` updates.
class Countdown : public nnh::Scene {
public:
    explicit Countdown(int frames) : left_(frames) {}
    void update() override {
        ++updates;
        if (--left_ == 0) nnh::scenes::pop();
    }
    int updates = 0;

private:
    int left_;
};

/// Pushes `pause` when START goes down (through the event bus), pops itself on B.
class Level : public nnh::Scene {
public:
    explicit Level(nnh::Scene* pause) : pause_(pause) {}
    void on_enter() override { start_sub_ = nnh::events::subscribe<&Level::on_button>(this); }
    void on_exit() override { start_sub_.reset(); }
    void update() override { ++updates; }
    void on_button(const nnh::ButtonPressed& e) {
        if (e.button == nnh::Button::Start) nnh::scenes::push(*pause_);
        if (e.button == nnh::Button::B) nnh::scenes::pop();
    }
    int updates = 0;

private:
    nnh::Scene* pause_;
    nnh::Subscription start_sub_;
};

class Pause : public nnh::Scene {
public:
    void update() override {
        ++updates;
        if (nnh::input::pressed(nnh::Button::A)) nnh::scenes::pop();
    }
    int updates = 0;
};

} // namespace

NNH_TEST(run_ends_when_the_stack_becomes_empty) {
    restart();
    Countdown scene(3);
    nnh::run(scene);
    CHECK(scene.updates == 3);
    CHECK(!scene.on_stack() && nnh::scenes::size() == 0);
    nnh::shutdown();
}

NNH_TEST(run_ends_and_exits_scenes_when_the_app_must_quit) {
    restart();
    Log log;
    RecordingScene scene('A', &log);
    fake::set_quit_requested(true);
    nnh::run(scene);
    CHECK(log.is("A+ A-"));
    CHECK(!scene.on_stack());
    nnh::shutdown();
}

NNH_TEST(a_change_requested_in_a_frame_applies_at_the_start_of_the_next) {
    restart();
    Log log;
    RecordingScene a('A', &log), b('B', &log);
    nnh::scenes::push(a);
    CHECK(nnh::frame());
    nnh::scenes::update_and_draw();
    nnh::scenes::replace(b);
    nnh::scenes::update_and_draw(); // still A this frame
    CHECK(nnh::scenes::top() == &a);
    CHECK(nnh::frame());
    CHECK(nnh::scenes::top() == &b);
    CHECK(log.is("A+ Au Ad Au Ad A- B+"));
    nnh::shutdown();
}

NNH_TEST(scenes_react_to_input_events_and_stack_a_pause_menu) {
    restart();
    Pause pause;
    Level level(&pause);
    nnh::scenes::push(level);
    CHECK(nnh::frame()); // level enters
    nnh::scenes::update_and_draw();
    fake::set_held_buttons(static_cast<std::uint32_t>(nnh::Button::Start));
    CHECK(nnh::frame()); // ButtonPressed{Start} -> push(pause) requested
    nnh::scenes::update_and_draw();
    fake::set_held_buttons(0);
    CHECK(nnh::frame()); // pause on top
    nnh::scenes::update_and_draw();
    CHECK(nnh::scenes::size() == 2 && nnh::scenes::top() == &pause);
    CHECK(level.updates == 2 && pause.updates == 1); // the level doesn't update under the pause menu
    fake::set_held_buttons(static_cast<std::uint32_t>(nnh::Button::A));
    CHECK(nnh::frame());
    nnh::scenes::update_and_draw(); // pause pops itself
    CHECK(nnh::frame());
    CHECK(nnh::scenes::top() == &level);
    nnh::shutdown();
}

NNH_TEST(shutdown_exits_scenes_left_on_the_stack) {
    restart();
    Log log;
    RecordingScene a('A', &log), b('B', &log);
    nnh::scenes::push(a);
    nnh::scenes::push(b);
    CHECK(nnh::frame());
    log.clear();
    nnh::shutdown();
    CHECK(log.is("B- A-"));
    CHECK(!a.on_stack() && !b.on_stack());
}

NNH_TEST(a_manual_loop_without_scenes_keeps_working) {
    restart();
    int frames = 0;
    while (nnh::frame() && frames < 5) ++frames;
    CHECK(frames == 5 && nnh::scenes::size() == 0);
    nnh::scenes::update_and_draw(); // no scenes: nothing happens
    nnh::shutdown();
}
