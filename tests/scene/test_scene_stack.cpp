// Scene stack: deferred push/pop/replace, enter/exit order, update and draw order, misuse (FR-8, FR-9).

#include <cstring>
#include <new>

#include "crash/crash_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "recording_scene.hpp"
#include "scene/scene_attachments.hpp"
#include "scene/scene_stack.hpp"
#include "test.hpp"

using nnh::detail::SceneStack;
using scene_test::Log;
using scene_test::RecordingScene;

namespace fake = nnh::platform_fake;

NNH_TEST(requests_wait_until_applied) {
    Log log;
    RecordingScene a('A', &log);
    SceneStack stack;
    stack.request_push(a);
    CHECK(stack.size() == 0 && log.is(""));
    stack.apply_pending();
    CHECK(stack.size() == 1 && stack.top() == &a && a.on_stack());
    CHECK(log.is("A+"));
    stack.clear();
}

NNH_TEST(changes_apply_in_request_order) {
    Log log;
    RecordingScene a('A', &log), b('B', &log), c('C', &log);
    SceneStack stack;
    stack.request_push(a);
    stack.request_push(b);
    stack.request_pop();
    stack.request_replace(c);
    stack.apply_pending();
    CHECK(log.is("A+ B+ B- A- C+"));
    CHECK(stack.size() == 1 && stack.top() == &c && !a.on_stack());
    stack.clear();
}

NNH_TEST(only_the_top_updates_and_all_draw_bottom_to_top) {
    Log log;
    RecordingScene level('L', &log), pause('P', &log);
    SceneStack stack;
    stack.request_push(level);
    stack.request_push(pause);
    stack.apply_pending();
    log.clear();
    stack.update_and_draw();
    CHECK(log.is("Pu Ld Pd"));
    stack.clear();
}

NNH_TEST(popping_the_top_resumes_the_scene_below) {
    Log log;
    RecordingScene level('L', &log), pause('P', &log);
    SceneStack stack;
    stack.request_push(level);
    stack.request_push(pause);
    stack.apply_pending();
    stack.request_pop();
    stack.apply_pending();
    log.clear();
    stack.update_and_draw();
    CHECK(log.is("Lu Ld"));
    stack.clear();
}

NNH_TEST(replace_on_an_empty_stack_pushes) {
    Log log;
    RecordingScene a('A', &log);
    SceneStack stack;
    stack.request_replace(a);
    stack.apply_pending();
    CHECK(log.is("A+") && stack.top() == &a);
    stack.clear();
}

NNH_TEST(clear_exits_every_scene_top_first_and_drops_pending) {
    Log log;
    RecordingScene a('A', &log), b('B', &log), c('C', &log);
    SceneStack stack;
    stack.request_push(a);
    stack.request_push(b);
    stack.apply_pending();
    stack.request_push(c); // never applied
    log.clear();
    stack.clear();
    CHECK(log.is("B- A-"));
    CHECK(stack.size() == 0 && stack.pending() == 0);
    // c is destroyed at the end of the test: it must not count as "waiting to be pushed"
}

namespace {

class PushOnEnter : public nnh::Scene {
public:
    PushOnEnter(SceneStack* stack, nnh::Scene* next) : stack_(stack), next_(next) {}
    void on_enter() override { stack_->request_push(*next_); }

private:
    SceneStack* stack_;
    nnh::Scene* next_;
};

class ExitRequestsPush : public nnh::Scene {
public:
    ExitRequestsPush(SceneStack* stack, nnh::Scene* next) : stack_(stack), next_(next) {}
    void on_exit() override { stack_->request_push(*next_); }

private:
    SceneStack* stack_;
    nnh::Scene* next_;
};

} // namespace

NNH_TEST(changes_requested_by_on_enter_apply_in_the_same_call) {
    Log log;
    SceneStack stack;
    RecordingScene b('B', &log);
    PushOnEnter a(&stack, &b);
    stack.request_push(a);
    stack.apply_pending();
    CHECK(stack.size() == 2 && stack.top() == &b && log.is("B+"));
    stack.clear();
}

NNH_TEST(changes_requested_by_on_exit_during_clear_are_dropped) {
    Log log;
    SceneStack stack;
    RecordingScene next('N', &log);
    ExitRequestsPush a(&stack, &next);
    stack.request_push(a);
    stack.apply_pending();
    stack.clear();
    CHECK(stack.size() == 0 && stack.pending() == 0 && log.is(""));
}

namespace {

/// An attachment that logs "<name>b" before its scene's update and "<name>x" after its scene's on_exit (D30).
struct Hooked : nnh::detail::SceneAttachment {
    Hooked(char n, Log* l) : name(n), log(l) {
        notify = [](nnh::detail::SceneAttachment& self, nnh::detail::SceneHook hook) {
            auto& h = static_cast<Hooked&>(self);
            h.log->add(h.name, hook == nnh::detail::SceneHook::BeforeUpdate ? 'b' : 'x');
        };
    }
    char name;
    Log* log;
};

} // namespace

NNH_TEST(attachments_run_before_the_top_update_only) {
    Log log;
    RecordingScene level('L', &log), pause('P', &log);
    Hooked on_level('l', &log), on_pause('p', &log);
    nnh::detail::scene_attach(level, on_level);
    nnh::detail::scene_attach(pause, on_pause);
    SceneStack stack;
    stack.request_push(level);
    stack.apply_pending();
    log.clear();
    stack.update_and_draw();
    CHECK(log.is("lb Lu Ld"));
    stack.request_push(pause);
    stack.apply_pending();
    log.clear();
    stack.update_and_draw(); // the level is under the pause menu: no hook, no update
    CHECK(log.is("pb Pu Ld Pd"));
    stack.clear();
    nnh::detail::scene_detach(level, on_level);
    nnh::detail::scene_detach(pause, on_pause);
}

NNH_TEST(attachments_hear_after_exit_on_pop_replace_and_clear) {
    Log log;
    RecordingScene a('A', &log), b('B', &log), c('C', &log);
    Hooked on_a('a', &log), on_b('b', &log), on_c('c', &log);
    nnh::detail::scene_attach(a, on_a);
    nnh::detail::scene_attach(b, on_b);
    nnh::detail::scene_attach(c, on_c);
    SceneStack stack;
    stack.request_push(a);
    stack.request_push(b);
    stack.request_pop();
    stack.request_replace(c);
    stack.apply_pending();
    CHECK(log.is("A+ B+ B- bx A- ax C+"));
    log.clear();
    stack.clear();
    CHECK(log.is("C- cx"));
    nnh::detail::scene_detach(a, on_a);
    nnh::detail::scene_detach(b, on_b);
    nnh::detail::scene_detach(c, on_c);
}

namespace {

SceneStack* g_stack = nullptr;
Log g_log;

void push_twice() {
    static RecordingScene a('A', &g_log);
    g_stack->request_push(a);
    g_stack->request_push(a);
    g_stack->apply_pending();
}

void pop_empty() {
    g_stack->request_pop();
    g_stack->apply_pending();
}

void destroy_while_on_stack() {
    alignas(RecordingScene) unsigned char storage[sizeof(RecordingScene)];
    auto* scene = new (storage) RecordingScene('A', &g_log);
    g_stack->request_push(*scene);
    g_stack->apply_pending();
    scene->~RecordingScene();
}

void destroy_while_pending() {
    alignas(RecordingScene) unsigned char storage[sizeof(RecordingScene)];
    auto* scene = new (storage) RecordingScene('A', &g_log);
    g_stack->request_push(*scene);
    scene->~RecordingScene();
}

void push_loop() {
    struct Loop : nnh::Scene {
        void on_enter() override { g_stack->request_pop(); }
        void on_exit() override { g_stack->request_push(*this); }
    };
    static Loop loop;
    g_stack->request_push(loop);
    g_stack->apply_pending();
}

void too_many_requests() {
    for (int i = 0; i <= nnh::scenes::kMaxPending; ++i) g_stack->request_pop();
}

// `tidy`: leave no static scene marked as on the stack (its destructor would fail at exit). Not for the
// tests whose scene lived in a stack frame that the crash unwound.
bool crashes(void (*fn)(), const char* message, bool tidy) {
    fake::reset();
    nnh::detail::crash_install();
    SceneStack stack;
    g_stack = &stack;
    const bool crashed = fake::run_expecting_crash(fn);
    const bool mentions = !nnh::build::kDevToolsEnabled || std::strstr(fake::last_crash().top, message) != nullptr;
    if (tidy) stack.clear();
    return crashed && mentions;
}

} // namespace

NNH_TEST(misuse_stops_the_game_with_a_clear_message) {
    CHECK(crashes(push_twice, "already on the scene stack", true));
    CHECK(crashes(pop_empty, "empty scene stack", true));
    CHECK(crashes(destroy_while_on_stack, "scene destroyed while still on the scene stack", false));
    CHECK(crashes(destroy_while_pending, "scene destroyed while still on the scene stack", false));
    CHECK(crashes(push_loop, "keep requesting more changes", true));
    CHECK(crashes(too_many_requests, "too many scene changes", true));
}
