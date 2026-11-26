// The console remote's language (spec 029 FR-27, D28): a Language message makes its code the system language from its
// frame on. A script owns the language like the slider: "en" from its Hello until its `language` command, back to "en"
// on `language off`; interactive control overrides the console's language only while a code is set; everything goes
// back to the console's own language when the PC leaves.

#include <cstring>

#include "nnh/platform/input.hpp"
#include "nnh/system.hpp"
#include "remote/remote_internal.hpp"
#include "remote_fixture.hpp"

using namespace remote_test;
using nnh::platform::InputSample;

namespace {

/// A connected console set to German.
void start() {
    fake::reset();
    fake::set_system_language("de");
    nnh::detail::remote_reset();
    fake::remote_connect();
}

void frame(std::uint64_t n) {
    InputSample s;
    nnh::detail::remote_frame_start(n, s);
}

wire::Message script_hello() {
    wire::Message m = hello();
    m.exclusive = true;
    return m;
}

bool language_is(const char* code) { return std::strcmp(nnh::system::language(), code) == 0; }

} // namespace

NNH_TEST(remote_language_interactive_overrides_until_released) {
    start();
    send(hello());
    frame(1);
    CHECK(language_is("de"));
    send(language(2, 2, "ja"));
    frame(2);
    CHECK(language_is("ja"));
    send(language(3, 3, nullptr));
    frame(3);
    CHECK(language_is("de"));
}

NNH_TEST(remote_language_a_script_owns_it_from_its_hello) {
    start();
    send(script_hello());
    frame(1);
    CHECK(language_is("en")); // the console's setting is ignored while the script runs
    send(language(2, 2, "pt-BR"));
    frame(2);
    CHECK(language_is("pt-BR"));
    send(language(3, 3, nullptr)); // `language off`: back to the script's default
    frame(3);
    CHECK(language_is("en"));
}

NNH_TEST(remote_language_applies_at_its_frame) {
    start();
    send(script_hello());
    send(language(2, 3, "ru"));
    frame(1);
    frame(2);
    CHECK(language_is("en"));
    frame(3);
    CHECK(language_is("ru"));
}

NNH_TEST(remote_language_goes_back_to_the_console_when_the_pc_leaves) {
    start();
    send(script_hello());
    send(language(2, 0, "ja"));
    send(bye(3, 2));
    frame(1);
    CHECK(language_is("ja"));
    frame(2);
    CHECK(language_is("de"));
    start();
    send(script_hello());
    send(language(2, 0, "ja"));
    frame(1);
    fake::remote_disconnect();
    frame(2);
    CHECK(language_is("de"));
}

NNH_TEST(remote_language_is_forgotten_by_a_reset) {
    start();
    send(hello());
    send(language(2, 0, "ko"));
    frame(1);
    CHECK(language_is("ko"));
    nnh::detail::remote_reset();
    CHECK(language_is("de"));
}
