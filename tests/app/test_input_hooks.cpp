// After-input hooks (decision D32, spec 017 T3.1): run in registration order once per nnh::frame(), after the
// frame's input is read and its input events queued, before dispatch; add/remove rules.

#include "core/input_hooks.hpp"
#include "crash/crash_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/events.hpp"
#include "nnh/input.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

using nnh::detail::InputHook;

namespace {

struct Probe {
    InputHook hook;
    int runs = 0;
    int order = 0;
    bool saw_a_pressed = false;
};

int g_counter = 0;

struct HookFired {
    int order;
};

Probe& probe_of(InputHook& h) { return *reinterpret_cast<Probe*>(&h); } // hook is the first member

void record(InputHook& h) {
    Probe& p = probe_of(h);
    ++p.runs;
    p.order = ++g_counter;
    p.saw_a_pressed = nnh::input::pressed(nnh::Button::A);
    nnh::events::publish(HookFired{p.order});
}

void restart() {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({}));
    g_counter = 0;
}

Probe g_twice;
void add_twice() {
    nnh::detail::add_input_hook(g_twice.hook);
    nnh::detail::add_input_hook(g_twice.hook);
}

} // namespace

NNH_TEST(no_input_hooks_leaves_the_list_empty) {
    restart();
    CHECK(nnh::detail::g_input_hooks == nullptr && nnh::detail::g_run_input_hooks == nullptr);
    CHECK(nnh::frame());
    nnh::shutdown();
}

NNH_TEST(input_hooks_run_once_per_frame_in_registration_order) {
    restart();
    Probe a{{nullptr, &record}}, b{{nullptr, &record}};
    nnh::detail::add_input_hook(a.hook);
    nnh::detail::add_input_hook(b.hook);
    CHECK(nnh::frame());
    CHECK(a.runs == 1 && b.runs == 1 && a.order == 1 && b.order == 2);
    CHECK(nnh::frame());
    CHECK(a.runs == 2 && b.runs == 2);
    nnh::detail::remove_input_hook(a.hook);
    CHECK(nnh::detail::g_run_input_hooks != nullptr); // b is left
    nnh::detail::remove_input_hook(b.hook);
    CHECK(nnh::detail::g_input_hooks == nullptr && nnh::detail::g_run_input_hooks == nullptr);
    nnh::shutdown();
}

NNH_TEST(an_input_hook_sees_this_frames_input) {
    restart();
    Probe a{{nullptr, &record}};
    nnh::detail::add_input_hook(a.hook);
    nnh::platform_fake::set_held_buttons(static_cast<std::uint32_t>(nnh::Button::A));
    CHECK(nnh::frame());
    CHECK(a.saw_a_pressed);
    nnh::detail::remove_input_hook(a.hook);
    nnh::shutdown();
}

NNH_TEST(events_queued_by_an_input_hook_follow_the_frames_input_events_in_the_same_dispatch) {
    restart();
    Probe a{{nullptr, &record}};
    nnh::detail::add_input_hook(a.hook);
    int seq = 0, button_at = 0, hook_at = 0;
    nnh::Subscription s1 = nnh::events::subscribe<nnh::ButtonPressed>([&](const nnh::ButtonPressed&) {
        button_at = ++seq;
    });
    nnh::Subscription s2 = nnh::events::subscribe<HookFired>([&](const HookFired&) { hook_at = ++seq; });
    nnh::platform_fake::set_held_buttons(static_cast<std::uint32_t>(nnh::Button::B));
    CHECK(nnh::frame()); // delivered inside this frame
    CHECK(button_at == 1 && hook_at == 2);
    nnh::detail::remove_input_hook(a.hook);
    nnh::shutdown();
}

NNH_TEST(removing_an_input_hook_keeps_the_others_and_ignores_absent_ones) {
    restart();
    Probe a{{nullptr, &record}}, b{{nullptr, &record}}, c{{nullptr, &record}};
    nnh::detail::add_input_hook(a.hook);
    nnh::detail::add_input_hook(b.hook);
    nnh::detail::add_input_hook(c.hook);
    nnh::detail::remove_input_hook(b.hook);
    nnh::detail::remove_input_hook(b.hook); // no-op
    CHECK(nnh::frame());
    CHECK(a.runs == 1 && b.runs == 0 && c.runs == 1 && a.order == 1 && c.order == 2);
    nnh::detail::remove_input_hook(a.hook);
    nnh::detail::remove_input_hook(c.hook);
    CHECK(nnh::detail::g_input_hooks == nullptr);
    nnh::shutdown();
}

NNH_TEST(adding_an_input_hook_twice_asserts) {
    if (!nnh::build::kAssertsEnabled) return;
    restart();
    g_twice = Probe{{nullptr, &record}};
    nnh::detail::crash_install();
    CHECK(nnh::platform_fake::run_expecting_crash(add_twice));
    nnh::detail::remove_input_hook(g_twice.hook);
    CHECK(nnh::detail::g_input_hooks == nullptr);
    nnh::shutdown();
}
