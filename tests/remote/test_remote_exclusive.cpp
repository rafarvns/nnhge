// Console remote session, exclusive mode (spec 013 amendment 2026-11-24): while a script is attached (its Hello
// has the exclusive flag) the game reads only the PC's controls; a console not at rest when it attaches is
// reported once. Interactive control (no flag) keeps the merge. Over the fake platform's in-memory PC.

#include <cstring>

#include "nnh/input.hpp"
#include "nnh/platform/input.hpp"
#include "remote/remote_internal.hpp"
#include "remote_fixture.hpp"

using namespace remote_test;
using nnh::LogLevel;
using nnh::platform::InputSample;

namespace {

constexpr std::uint32_t kA = static_cast<std::uint32_t>(nnh::Button::A);
constexpr std::uint32_t kB = static_cast<std::uint32_t>(nnh::Button::B);

void start() {
    fake::reset();
    nnh::detail::remote_reset();
    fake::remote_connect();
}

wire::Message script_hello(std::uint16_t seq = 1) {
    wire::Message m = hello(seq);
    m.exclusive = true;
    return m;
}

/// An Old 3DS whose Circle Pad rests at raw (10, 14) of 156, with A held and the screen touched.
InputSample busy_console() {
    InputSample real;
    real.buttons = kA;
    real.circle_x = 10.0f / 156.0f;
    real.circle_y = 14.0f / 156.0f;
    real.touching = true;
    real.touch_x = 300;
    real.touch_y = 10;
    return real;
}

/// A console whose Circle Pad rests at raw (10, 14) of 156 and nothing else.
InputSample resting_pad() {
    InputSample real;
    real.circle_x = 10.0f / 156.0f;
    real.circle_y = 14.0f / 156.0f;
    return real;
}

/// Runs the start of `frame` on the console's own reading `real` and returns what the game would read.
InputSample at(std::uint64_t frame, InputSample real) {
    nnh::detail::remote_frame_start(frame, real);
    return real;
}

bool idle(const InputSample& s) {
    return s.buttons == 0 && s.circle_x == 0.0f && s.circle_y == 0.0f && s.cstick_x == 0.0f && s.cstick_y == 0.0f &&
           !s.touching;
}

/// Counts the warnings about the console's own controls while alive; keeps the last one.
struct RestWarnings {
    int count = 0;
    char last[nnh::log::kMaxLineLength] = {};

    RestWarnings() { nnh::log::add_sink(&RestWarnings::sink, this); }
    ~RestWarnings() { nnh::log::remove_sink(&RestWarnings::sink, this); }
    RestWarnings(const RestWarnings&) = delete;
    RestWarnings& operator=(const RestWarnings&) = delete;

    static void sink(LogLevel level, const char* line, void* user) {
        auto* self = static_cast<RestWarnings*>(user);
        if (level != LogLevel::Warn || std::strstr(line, "ignored while the script runs") == nullptr) return;
        ++self->count;
        std::strncpy(self->last, line, sizeof self->last - 1);
    }
};

} // namespace

NNH_TEST(remote_exclusive_a_script_hides_the_consoles_own_controls) {
    start();
    send(script_hello());
    CHECK(idle(at(1, busy_console())));
    CHECK(idle(at(2, busy_console())));
}

NNH_TEST(remote_exclusive_the_game_reads_exactly_the_scripts_controls) {
    start();
    send(script_hello());
    send(buttons(2, 0, kB));
    send(stick(3, 0, 0, 0, 50)); // less than the resting pad: still the script's
    const InputSample out = at(1, busy_console());
    CHECK(out.buttons == kB);
    CHECK(out.circle_x == 0.0f && out.circle_y == 0.05f);
    CHECK(!out.touching);
}

NNH_TEST(remote_exclusive_interactive_control_still_merges) {
    start();
    send(hello()); // no flag: a person typing commands
    send(buttons(2, 0, kB));
    const InputSample out = at(1, busy_console());
    CHECK(out.buttons == (kA | kB));
    CHECK(out.circle_x > 0.0f && out.touching && out.touch_x == 300);
}

NNH_TEST(remote_exclusive_before_the_hello_the_console_reads_its_own_controls) {
    start();
    const InputSample out = at(1, busy_console());
    CHECK(out.buttons == kA && out.touching);
}

NNH_TEST(remote_exclusive_ends_with_the_bye) {
    start();
    send(script_hello());
    send(bye(2, 3));
    CHECK(idle(at(1, busy_console())));
    CHECK(idle(at(2, busy_console())));
    const InputSample after = at(3, busy_console()); // the Bye applies at the start of frame 3
    CHECK(after.buttons == kA && after.touching);
}

NNH_TEST(remote_exclusive_ends_when_the_pc_disconnects) {
    start();
    send(script_hello());
    CHECK(idle(at(1, busy_console())));
    fake::remote_disconnect();
    CHECK(at(2, busy_console()).buttons == kA);
}

NNH_TEST(remote_exclusive_ends_when_the_remote_turns_off) {
    start();
    send(script_hello());
    CHECK(idle(at(1, busy_console())));
    for (int i = 0; i < 8; ++i) send_garbage(static_cast<std::uint16_t>(100 + i));
    CHECK(at(2, busy_console()).buttons == kA);
}

NNH_TEST(remote_exclusive_a_later_plain_hello_goes_back_to_merging) {
    start();
    send(script_hello());
    CHECK(idle(at(1, busy_console())));
    send(hello(2));
    CHECK(at(2, busy_console()).buttons == kA);
}

NNH_TEST(remote_exclusive_warns_once_about_a_pad_not_at_rest) {
    start();
    RestWarnings warnings;
    send(script_hello());
    at(1, resting_pad());
    CHECK(warnings.count == 1);
    CHECK(std::strstr(warnings.last, "Circle Pad not at rest (0.064, 0.090)") != nullptr);
    at(2, resting_pad());
    at(3, resting_pad());
    CHECK(warnings.count == 1);
}

NNH_TEST(remote_exclusive_warns_about_each_busy_control) {
    start();
    RestWarnings warnings;
    InputSample real = busy_console();
    real.cstick_y = -0.5f;
    send(script_hello());
    at(1, real);
    CHECK(warnings.count == 4); // Circle Pad, C-Stick, buttons, touch
}

NNH_TEST(remote_exclusive_names_held_buttons_and_the_touch) {
    start();
    RestWarnings warnings;
    InputSample real;
    real.buttons = kB;
    send(script_hello());
    at(1, real);
    CHECK(warnings.count == 1 && std::strstr(warnings.last, "buttons held (0x0002)") != nullptr);
    start();
    InputSample touched;
    touched.touching = true;
    touched.touch_x = 12;
    touched.touch_y = 34;
    send(script_hello());
    at(1, touched);
    CHECK(warnings.count == 2 && std::strstr(warnings.last, "screen touched (12, 34)") != nullptr);
}

NNH_TEST(remote_exclusive_a_console_at_rest_is_silent) {
    start();
    RestWarnings warnings;
    InputSample real;
    real.circle_x = 0.05f; // inside the dead zone
    send(script_hello());
    at(1, real);
    at(2, busy_console()); // controls touched after the attach aren't reported
    CHECK(warnings.count == 0);
}

NNH_TEST(remote_exclusive_interactive_control_never_warns) {
    start();
    RestWarnings warnings;
    send(hello());
    at(1, busy_console());
    CHECK(warnings.count == 0);
}

NNH_TEST(remote_exclusive_each_script_attach_checks_again) {
    start();
    RestWarnings warnings;
    send(script_hello());
    send(bye(2, 2));
    at(1, resting_pad());
    at(2, resting_pad());
    send(script_hello(3));
    at(3, resting_pad());
    CHECK(warnings.count == 2);
}
