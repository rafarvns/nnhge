// The actions example's self-check (spec 017 T6.4, AC-8): the verdict from what the console script did, and the
// fixed log lines it expects.

#include "actions/action_log.hpp"
#include "actions/self_check.hpp"
#include "test.hpp"

#include <cstring>

using nnh::Button;
using nnh::Resolution;
using nnh::Source;

namespace {
bool says(const ActionsSelfCheck& c, const char* text) {
    char out[96];
    c.verdict(out, sizeof out);
    return std::strstr(out, text) != nullptr;
}

/// The script's steps, up to (not including) the bench.
void play_script(ActionsSelfCheck& c) {
    c.remap("jump", Source::button(Button::B), Resolution::Swapped, "attack");
    c.remap("jump", Source::button(Button::Y), Resolution::None, "");
    c.saved(0x1234u);
    c.pressed("jump");
    c.pressed("attack");
    c.reloaded(0x1234u, 2, 0);
}
} // namespace

NNH_TEST(the_self_check_passes_when_every_step_happened) {
    ActionsSelfCheck c;
    play_script(c);
    CHECK(!c.done());
    c.bench(0.05f, /*console=*/true);
    CHECK(c.done() && c.passed() && says(c, "actions: PASS 7/7"));
}

NNH_TEST(the_self_check_names_the_first_missing_step) {
    ActionsSelfCheck c;
    c.remap("jump", Source::button(Button::B), Resolution::Replaced, "attack"); // not a swap
    c.remap("jump", Source::button(Button::Y), Resolution::None, "");
    c.bench(0.01f, false);
    CHECK(!c.passed() && says(c, "actions: FAIL no swap"));
}

NNH_TEST(the_self_check_wants_the_presses_after_the_save) {
    ActionsSelfCheck c;
    c.remap("jump", Source::button(Button::B), Resolution::Swapped, "attack");
    c.remap("jump", Source::button(Button::Y), Resolution::None, "");
    c.pressed("jump");
    c.saved(1);
    c.pressed("attack");
    c.reloaded(1, 2, 0);
    c.bench(0.01f, false);
    CHECK(says(c, "actions: FAIL no jump on the new button"));
}

NNH_TEST(the_self_check_fails_when_the_reload_differs) {
    ActionsSelfCheck c;
    c.remap("jump", Source::button(Button::B), Resolution::Swapped, "attack");
    c.remap("jump", Source::button(Button::Y), Resolution::None, "");
    c.saved(5);
    c.pressed("jump");
    c.pressed("attack");
    c.reloaded(6, 2, 0);
    c.bench(0.01f, false);
    CHECK(says(c, "actions: FAIL reload"));
}

NNH_TEST(the_bench_budget_applies_on_the_console_only) {
    ActionsSelfCheck pc, console, fast_console, unmeasured;
    play_script(pc);
    play_script(console);
    play_script(fast_console);
    play_script(unmeasured);
    pc.bench(0.5f, false);
    console.bench(0.2f, true);         // the Old 3DS budget is < 0.2 ms (measured 0.142 ms, 2026-11-22)
    fast_console.bench(0.19f, true);
    unmeasured.bench(-1.0f, false);
    CHECK(pc.passed());
    CHECK(!console.passed() && says(console, "actions: FAIL bench"));
    CHECK(fast_console.passed());
    CHECK(!unmeasured.passed());
}

NNH_TEST(the_example_logs_fixed_lines) {
    char out[96];
    format_edge(out, sizeof out, "jump", true);
    CHECK(std::strcmp(out, "actions: jump pressed") == 0);
    format_edge(out, sizeof out, "attack", false);
    CHECK(std::strcmp(out, "actions: attack released") == 0);
    CHECK(std::strcmp(aim_line(false), "actions: aim uses L/R (no C-Stick)") == 0);
    CHECK(bindings_sum("", 0) == 0x811C9DC5u && bindings_sum("a", 1) == 0xE40C292Cu);
}

NNH_TEST(remap_lines_name_the_actions_and_the_control) {
    nnh::ActionSet set{{.actions = 2}};
    CHECK(set.add(nnh::ActionId(0), {.name = "jump", .bindings = {{Source::button(Button::A)}}}));
    CHECK(set.add(nnh::ActionId(1), {.name = "attack", .bindings = {{Source::button(Button::B)}}}));
    nnh::RemapResult r = set.set_source(nnh::ActionId(0), 0, Source::button(Button::B));
    char out[96];
    format_remap(out, sizeof out, r, set);
    CHECK(std::strcmp(out, "actions: rebind jump -> B (swapped with attack)") == 0);
    r = set.set_source(nnh::ActionId(0), 0, Source::button(Button::Y));
    format_remap(out, sizeof out, r, set);
    CHECK(std::strcmp(out, "actions: rebind jump -> Y") == 0);
    r = set.set_source(nnh::ActionId(0), 0, Source::button(Button::A), nnh::ConflictPolicy::Reject);
    format_remap(out, sizeof out, r, set);
    CHECK(std::strcmp(out, "actions: rebind jump rejected (A is used by attack)") == 0);
    r.state = nnh::ListenState::Cancelled;
    format_remap(out, sizeof out, r, set);
    CHECK(std::strcmp(out, "actions: rebind jump cancelled") == 0);
}
