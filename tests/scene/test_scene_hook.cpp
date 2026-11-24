// The scene stack's transition hook (spec 023 T4.2, decision D38): the stack's halves, the tolerant pop, delegation
// of scenes::update_and_draw(), run()'s loop condition and the cancel before the stack is cleared.

#include <cstring>

#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"
#include "recording_scene.hpp"
#include "scene/scene_hook.hpp"
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

struct HookCounts {
    int frames = 0;
    int cancels = 0;
    int clear_after = -1; ///< the hook removes itself after this many frames (-1: never)
};
HookCounts g_counts;

void hook_frame() {
    ++g_counts.frames;
    if (g_counts.frames == g_counts.clear_after) nnh::detail::g_scene_transition = nullptr;
}
void hook_cancel() {
    ++g_counts.cancels;
    nnh::detail::g_scene_transition = nullptr;
}
constexpr nnh::detail::SceneTransitionHook kTestHook{&hook_frame, &hook_cancel};

/// Pops itself on its first update and installs the test hook.
class LastScene : public nnh::Scene {
public:
    void update() override {
        ++updates;
        nnh::scenes::pop();
        nnh::detail::g_scene_transition = &kTestHook;
    }
    int updates = 0;
};

} // namespace

NNH_TEST(the_stack_halves_make_the_same_calls_as_update_and_draw) {
    restart();
    Log log;
    RecordingScene a('A', &log), b('B', &log);
    nnh::scenes::push(a);
    nnh::scenes::push(b);
    CHECK(nnh::frame());
    log.clear();
    nnh::scenes::update_and_draw();
    char whole[64];
    std::strcpy(whole, log.text);
    log.clear();
    nnh::detail::scenes_update_top();
    nnh::detail::scenes_draw_all();
    CHECK(std::strcmp(whole, "Bu Ad Bd") == 0 && log.is("Bu Ad Bd"));
    nnh::shutdown();
}

NNH_TEST(the_halves_do_nothing_on_an_empty_stack) {
    restart();
    CHECK(nnh::frame());
    nnh::detail::scenes_update_top();
    nnh::detail::scenes_draw_all();
    CHECK(nnh::scenes::size() == 0);
    nnh::shutdown();
}

NNH_TEST(a_transition_change_goes_through_the_deferred_path) {
    restart();
    Log log;
    RecordingScene a('A', &log), b('B', &log), c('C', &log);
    nnh::scenes::push(a);
    CHECK(nnh::frame());
    nnh::detail::scenes_request_change(nnh::detail::SceneChange::Push, &b);
    CHECK(nnh::scenes::top() == &a); // deferred
    CHECK(nnh::frame());
    CHECK(nnh::scenes::top() == &b);
    nnh::detail::scenes_request_change(nnh::detail::SceneChange::Replace, &c);
    CHECK(nnh::frame());
    CHECK(nnh::scenes::top() == &c && nnh::scenes::size() == 2);
    nnh::detail::scenes_request_change(nnh::detail::SceneChange::TransitionPop, nullptr);
    CHECK(nnh::frame());
    CHECK(nnh::scenes::top() == &a);
    CHECK(log.is("A+ B+ B- C+ C-"));
    nnh::shutdown();
}

NNH_TEST(a_transition_pop_on_an_empty_stack_pops_nothing) {
    restart();
    nnh::detail::scenes_request_change(nnh::detail::SceneChange::TransitionPop, nullptr);
    CHECK(nnh::frame()); // no crash: spec 001's pop would stop the game here
    CHECK(nnh::scenes::size() == 0);
    nnh::shutdown();
}

NNH_TEST(update_and_draw_delegates_to_the_hook_while_it_is_set) {
    restart();
    Log log;
    RecordingScene a('A', &log);
    nnh::scenes::push(a);
    CHECK(nnh::frame());
    log.clear();
    g_counts = {};
    nnh::detail::g_scene_transition = &kTestHook;
    nnh::scenes::update_and_draw();
    nnh::scenes::update_and_draw();
    CHECK(g_counts.frames == 2 && log.is(""));
    nnh::detail::g_scene_transition = nullptr;
    nnh::scenes::update_and_draw();
    CHECK(log.is("Au Ad"));
    nnh::shutdown();
}

NNH_TEST(shutdown_cancels_the_hook_before_clearing_the_stack) {
    restart();
    g_counts = {};
    nnh::detail::g_scene_transition = &kTestHook;
    nnh::shutdown();
    CHECK(g_counts.cancels == 1 && nnh::detail::g_scene_transition == nullptr);
}

NNH_TEST(run_keeps_looping_while_the_hook_runs_on_an_empty_stack) {
    restart();
    g_counts = {};
    g_counts.clear_after = 5;
    LastScene scene;
    nnh::run(scene);
    CHECK(scene.updates == 1 && g_counts.frames == 5 && g_counts.cancels == 0);
    nnh::shutdown();
}

NNH_TEST(the_end_of_run_cancels_a_running_hook) {
    restart();
    g_counts = {};
    // The hook never clears itself: run() ends when the system asks to quit, cancelling it first.
    struct Quitter {
        static void frame() {
            if (++g_counts.frames == 3) fake::set_quit_requested(true);
        }
    };
    static constexpr nnh::detail::SceneTransitionHook kQuitHook{&Quitter::frame, &hook_cancel};
    class Starter : public nnh::Scene {
        void update() override { nnh::detail::g_scene_transition = &kQuitHook; }
    } starter;
    nnh::run(starter);
    CHECK(g_counts.frames == 3 && g_counts.cancels == 1 && !starter.on_stack());
    nnh::shutdown();
}

NNH_TEST(a_waiting_scene_count_is_the_pending_push_count) {
    Log log;
    RecordingScene a('A', &log);
    CHECK(nnh::detail::scene_pending_count(a) == 0);
    ++nnh::detail::scene_pending_count(a);
    CHECK(nnh::detail::scene_pending_count(a) == 1);
    --nnh::detail::scene_pending_count(a);
}
