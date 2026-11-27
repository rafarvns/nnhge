// The console remote's 3D slider (spec 026 FR-16, plan P20, T11.3): message type 7 on the wire, and the session that
// makes it the engine's slider. A script owns the slider like the other controls (spec 013 amendment 2026-11-24): 0
// from its Hello until its `slider` command, back to 0 on `slider off`; interactive control overrides the console's
// slider only while a value is set; everything goes back to the console's own slider when the PC leaves.

#include "nnh/platform/input.hpp"
#include "nnh/system.hpp"
#include "remote/remote_internal.hpp"
#include "remote_fixture.hpp"

using namespace remote_test;
using nnh::Capability;
using nnh::platform::InputSample;

namespace {

struct Bytes {
    std::uint8_t b[wire::kMessageSize];
};

/// A Slider message byte by byte: seq 0x0201, frame 0x0A0B, release flag, value (little-endian).
Bytes slider_bytes(std::uint8_t release, std::uint16_t value) {
    return {{0x52, 7, 0x01, 0x02, 0x0B, 0x0A, 0, 0, release, 0, static_cast<std::uint8_t>(value),
             static_cast<std::uint8_t>(value >> 8), 0, 0, 0, 0}};
}

wire::Reject decode(const Bytes& m, wire::Message& out) { return wire::decode_message(m.b, out); }

/// A connected console with a stereo screen whose own slider reads 0.6.
void start() {
    fake::reset();
    fake::set_capabilities({Capability::SecondScreen, Capability::Stereo3D});
    fake::set_stereo_slider(0.6f);
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

} // namespace

NNH_TEST(remote_slider_message_decodes_and_round_trips) {
    wire::Message out;
    CHECK(decode(slider_bytes(0, 1000), out) == wire::Reject::None);
    CHECK(out.type == wire::MessageType::Slider && out.slider == 1000);
    CHECK(decode(slider_bytes(1, 0), out) == wire::Reject::None && out.slider == wire::kSliderRelease);
    std::uint8_t again[wire::kMessageSize];
    wire::encode_message(out, again);
    CHECK(std::memcmp(again, slider_bytes(1, 0).b, sizeof again) == 0);
    CHECK(decode(slider_bytes(0, 250), out) == wire::Reject::None);
    wire::encode_message(out, again);
    CHECK(std::memcmp(again, slider_bytes(0, 250).b, sizeof again) == 0);
}

NNH_TEST(remote_slider_message_refuses_bad_values) {
    wire::Message out;
    CHECK(decode(slider_bytes(0, 1001), out) == wire::Reject::Range);
    CHECK(decode(slider_bytes(2, 0), out) == wire::Reject::Range);
    CHECK(decode(slider_bytes(1, 5), out) == wire::Reject::Range); // released with a value
    Bytes extra = slider_bytes(0, 10);
    extra.b[12] = 1;
    CHECK(decode(extra, out) == wire::Reject::Range);
    Bytes pad = slider_bytes(0, 10);
    pad.b[9] = 1;
    CHECK(decode(pad, out) == wire::Reject::Range);
    Bytes type18 = slider_bytes(0, 10);
    type18.b[1] = 18; // 8 is the language (spec 029), 9–11 live values (spec 030), 12–17 reloads (spec 031)
    CHECK(decode(type18, out) == wire::Reject::Type);
}

NNH_TEST(remote_slider_interactive_overrides_until_released) {
    start();
    send(hello());
    frame(1);
    CHECK(nnh::system::stereo_slider() == 0.6f); // nothing set yet: the console's own
    send(slider(2, 2, 250));
    frame(2);
    CHECK(nnh::system::stereo_slider() == 0.25f);
    send(slider(3, 3, 0, true));
    frame(3);
    CHECK(nnh::system::stereo_slider() == 0.6f);
}

NNH_TEST(remote_slider_a_script_owns_it_from_its_hello) {
    start();
    send(script_hello());
    frame(1);
    CHECK(nnh::system::stereo_slider() == 0.0f); // the console's slider is ignored while the script runs
    send(slider(2, 2, 1000));
    frame(2);
    CHECK(nnh::system::stereo_slider() == 1.0f);
    send(slider(3, 3, 0, true)); // `slider off`: back to the script's default
    frame(3);
    CHECK(nnh::system::stereo_slider() == 0.0f);
}

NNH_TEST(remote_slider_applies_at_its_frame) {
    start();
    send(script_hello());
    send(slider(2, 3, 500));
    frame(1);
    frame(2);
    CHECK(nnh::system::stereo_slider() == 0.0f);
    frame(3);
    CHECK(nnh::system::stereo_slider() == 0.5f);
}

NNH_TEST(remote_slider_goes_back_to_the_console_when_the_pc_leaves) {
    start();
    send(script_hello());
    send(slider(2, 0, 1000));
    send(bye(3, 2));
    frame(1);
    CHECK(nnh::system::stereo_slider() == 1.0f);
    frame(2);
    CHECK(nnh::system::stereo_slider() == 0.6f);
    start();
    send(script_hello());
    send(slider(2, 0, 1000));
    frame(1);
    fake::remote_disconnect();
    frame(2);
    CHECK(nnh::system::stereo_slider() == 0.6f);
}
